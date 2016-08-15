/*
  This file is part of libudev-compat.
   
  Copyright 2015 Jude Nelson (judecn@gmail.com)

  libudev-compat is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  libudev-compat is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with libudev-compat; If not, see <http://www.gnu.org/licenses/>.
*/

#include "libudev-fs.h"
#include "libudev-private.h"
#include "log.h"

#define UDEV_FS_WATCH_DIR_FLAGS (IN_CREATE | IN_ONESHOT)

#ifdef TEST
#define UDEV_FS_EVENTS_DIR      "/tmp/events"
#else
#define UDEV_FS_EVENTS_DIR      "/dev/metadata/udev/events"
#endif

// how many monitors can there be per process?
// this is probably big enough.
// email me if it isn't.
#ifndef UDEV_MAX_MONITORS
#define UDEV_MAX_MONITORS 32768
#endif

static int udev_monitor_fs_events_path(char const *name, char *pathbuf,
				       int nonce);

// We need to make sure that on fork, a udev_monitor listening to the underlying filesystem
// will listen to its *own* process's events directory, at all times.  To do this, we will
// track every monitor that exists, by holding a reference internally.  When the process
// forks, we'll re-initialize all monitors to point to the new process's events directory.
static struct udev_monitor *g_monitor_table[UDEV_MAX_MONITORS];
static int g_monitor_lock = 0;	// primitive mutex that can work across forks
static pid_t g_pid = 0;		// tells us when to re-target the monitors

// spin-lock to lock the monitor table
static void g_monitor_table_spinlock()
{

	bool locked = false;
	while (1) {

		locked = __sync_bool_compare_and_swap(&g_monitor_lock, 0, 1);
		if (locked) {
			break;
		}
	}
}

// unlock the monitor table 
static void g_monitor_table_unlock()
{

	g_monitor_lock = 0;

	// make sure spinners see it.
	__sync_synchronize();
}

// on fork(), create a new events directory for each of this process's monitors
// and point them all to them.  This way, both the parent and child can continue 
// to receive device packets.
// NOTE: can only call async-safe methods
static void udev_monitor_atfork(void)
{

	int errsv = errno;
	int rc = 0;
	int i = 0;
	int cnt = 0;
	pid_t pid = getpid();
	struct udev_monitor *monitor = NULL;
	int socket_fds[2];
	struct epoll_event ev;

	write(STDERR_FILENO, "forked! begin split\n",
	      strlen("forked! begin split\n"));

	memset(&ev, 0, sizeof(struct epoll_event));

	// reset each monitor's inotify fd to point to a new PID-specific directory instead
	g_monitor_table_spinlock();

	if (g_pid != pid) {

		// child; do the fork 
		for (i = 0; i < UDEV_MAX_MONITORS; i++) {

			if (g_monitor_table[i] == NULL) {
				continue;
			}

			monitor = g_monitor_table[i];

			if (monitor->type != UDEV_MONITOR_TYPE_UDEV) {
				continue;
			}

			if (monitor->inotify_fd < 0) {
				continue;
			}

			if (monitor->epoll_fd < 0) {
				continue;
			}

			if (monitor->events_wd < 0) {
				continue;
			}
			// reset the socket buffer--the parent will be said to have 
			// received intermittent events before the child was created.
			if (monitor->sock >= 0) {

				// stop watching this socket--we'll regenerate it later
				epoll_ctl(monitor->epoll_fd, EPOLL_CTL_DEL,
					  monitor->sock, NULL);
				close(monitor->sock);
				monitor->sock = -1;
			}

			if (monitor->sock_fs >= 0) {
				close(monitor->sock_fs);
				monitor->sock_fs = -1;
			}

			rc = socketpair(AF_LOCAL,
					SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC,
					0, socket_fds);
			if (rc < 0) {

				// not much we can do here, except log an error 
				write(STDERR_FILENO,
				      "Failed to generate a socketpair\n",
				      strlen
				      ("Failed to generate a socketpair\n"));

				udev_monitor_fs_shutdown(monitor);
				g_monitor_table[i] = NULL;
				continue;
			}
			// child's copy of the monitor has its own socketpair 
			monitor->sock = socket_fds[0];
			monitor->sock_fs = socket_fds[1];

			// reinstall its filter 
			udev_monitor_filter_update(monitor);

			// watch the child's socket 
			ev.events = EPOLLIN;
			ev.data.fd = monitor->sock;
			rc = epoll_ctl(monitor->epoll_fd, EPOLL_CTL_ADD,
				       monitor->sock, &ev);
			if (rc < 0) {

				// not much we can do here, except log an error 
				write(STDERR_FILENO,
				      "Failed to add monitor socket\n",
				      strlen("Failed to add monitor socket\n"));

				udev_monitor_fs_shutdown(monitor);
				g_monitor_table[i] = NULL;
				continue;
			}
			// reset the inotify watch
			rc = inotify_rm_watch(monitor->inotify_fd,
					      monitor->events_wd);
			monitor->events_wd = -1;

			if (rc < 0) {

				rc = -errno;
				if (rc == -EINVAL) {

					// monitor->events_wd was invalid 
					rc = 0;
				} else if (rc == -EBADF) {

					// monitor->inotify_fd is invalid.
					// not much we can do here, except log an error 
					write(STDERR_FILENO,
					      "Invalid inotify handle\n",
					      strlen("Invalid inotify handle"));

					udev_monitor_fs_shutdown(monitor);
					g_monitor_table[i] = NULL;
					continue;
				}
			}

			if (rc == 0) {

				udev_monitor_fs_events_path("",
							    monitor->events_dir,
							    i);

				// try to create a new directory for this monitor
				rc = mkdir(monitor->events_dir, 0700);

				if (rc < 0) {
					// failed, we have.
					// child will not get any notifications from this monitor
					rc = -errno;
					write(STDERR_FILENO, "Failed to mkdir ",
					      strlen("Failed to mkdir "));
					write(STDERR_FILENO,
					      monitor->events_dir,
					      strlen(monitor->events_dir));
					write(STDERR_FILENO, "\n", 1);

					udev_monitor_fs_shutdown(monitor);
					g_monitor_table[i] = NULL;
					return;
				}
				// reconnect to the new directory
				monitor->events_wd =
				    inotify_add_watch(monitor->inotify_fd,
						      monitor->events_dir,
						      UDEV_FS_WATCH_DIR_FLAGS);
				if (monitor->events_wd < 0) {

					// there's not much we can safely do here, besides log an error
					write(STDERR_FILENO, "Failed to watch ",
					      strlen("Failed to watch "));
					write(STDERR_FILENO,
					      monitor->events_dir,
					      strlen(monitor->events_dir));
					write(STDERR_FILENO, "\n", 1);
				}
			} else {

				// there's not much we can safely do here, besides log an error
				rc = -errno;
				write(STDERR_FILENO, "Failed to disconnect!\n",
				      strlen("Failed to disconnect!\n"));
			}
		}

		g_pid = pid;
	}

	g_monitor_table_unlock();

	write(STDERR_FILENO, "end atfork()\n", strlen("end atfork()\n"));

	// restore...
	errno = errsv;
}

// register a monitor in our list of monitors 
// return 0 on success
// return -ENOSPC if we're out of slots
static int udev_monitor_register(struct udev_monitor *monitor)
{

	g_monitor_table_spinlock();

	// find a free slot 
	int i = 0;
	int rc = -ENOSPC;
	for (i = 0; i < UDEV_MAX_MONITORS; i++) {

		if (g_monitor_table[i] == NULL) {

			g_monitor_table[i] = monitor;
			monitor->slot = i;
			rc = 0;
			break;
		}
	}

	if (g_pid == 0) {

		// first monitor ever.
		// register our fork handler.
		g_pid = getpid();
		pthread_atfork(NULL, NULL, udev_monitor_atfork);
	}

	g_monitor_table_unlock();

	return rc;
}

// unregister a monitor in our list of monitors 
static void udev_monitor_unregister(struct udev_monitor *monitor)
{

	if (monitor->slot < 0) {
		return;
	}

	g_monitor_table_spinlock();

	g_monitor_table[monitor->slot] = NULL;

	g_monitor_table_unlock();

	monitor->slot = -1;
}

// write, but mask EINTR
// return number of bytes written on success 
// return -errno on I/O error
ssize_t udev_write_uninterrupted(int fd, char const *buf, size_t len)
{

	ssize_t num_written = 0;

	if (buf == NULL) {
		return -EINVAL;
	}

	while ((unsigned)num_written < len) {
		ssize_t nw = write(fd, buf + num_written, len - num_written);
		if (nw < 0) {

			int errsv = -errno;
			if (errsv == -EINTR) {
				continue;
			}

			return errsv;
		}
		if (nw == 0) {
			break;
		}

		num_written += nw;
	}

	return num_written;
}

// read, but mask EINTR
// return number of bytes read on success 
// return -errno on I/O error
// NOTE: must be async-safe!
ssize_t udev_read_uninterrupted(int fd, char *buf, size_t len)
{

	ssize_t num_read = 0;

	if (buf == NULL) {
		return -EINVAL;
	}

	while ((unsigned)num_read < len) {
		ssize_t nr = read(fd, buf + num_read, len - num_read);
		if (nr < 0) {

			int errsv = -errno;
			if (errsv == -EINTR) {
				continue;
			}

			return errsv;
		}
		if (nr == 0) {
			break;
		}

		num_read += nr;
	}

	return num_read;
}

// set up a filesystem monitor
// register pthread_atfork() handlers to ensure that its children 
// get their own filesystem monitor state.
// * set up /dev/events/libudev-$PID/
// * start watching /dev/events/libudev-$PID for new files
int udev_monitor_fs_setup(struct udev_monitor *monitor)
{

	int rc = 0;
	struct epoll_event ev;

	monitor->inotify_fd = -1;
	monitor->epoll_fd = -1;
	monitor->sock = -1;
	monitor->sock_fs = -1;
	monitor->slot = -1;

	int socket_fd[2] = { -1, -1 };

	memset(&monitor->events_dir, 0, PATH_MAX + 1);
	memset(&ev, 0, sizeof(struct epoll_event));

	// make sure this monitor can't disappear on us
	udev_monitor_register(monitor);

	// set up inotify 
	monitor->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (monitor->inotify_fd < 0) {

		rc = -errno;
		log_error("inotify_init rc = %d", rc);

		udev_monitor_fs_shutdown(monitor);
		return rc;
	}
	// epoll descriptor unifying inotify and event counter
	monitor->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (monitor->epoll_fd < 0) {

		rc = -errno;
		log_error("epoll_create rc = %d", rc);

		udev_monitor_fs_shutdown(monitor);
		return rc;
	}
	// create our monitor directory /dev/events/libudev-$PID
	udev_monitor_fs_events_path("", monitor->events_dir, monitor->slot);
	rc = mkdir(monitor->events_dir, 0700);
	if (rc != 0) {

		rc = -errno;
		log_error("mkdir('%s') rc = %d", monitor->events_dir, rc);

		udev_monitor_fs_destroy(monitor);
		return rc;
	}
	// begin watching /dev/events/libudev-$PID
	monitor->events_wd =
	    inotify_add_watch(monitor->inotify_fd, monitor->events_dir,
			      UDEV_FS_WATCH_DIR_FLAGS);
	if (monitor->events_wd < 0) {

		rc = -errno;
		log_error("inotify_add_watch('%s') rc = %d",
			  monitor->events_dir, rc);

		udev_monitor_fs_destroy(monitor);
		return rc;
	}
	// set up local socket pair with the parent process
	// needs to be a socket (not a pipe) since we're going to attach a BPF to it.
	rc = socketpair(AF_LOCAL, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
			socket_fd);
	if (rc < 0) {

		rc = -errno;
		log_error
		    ("socketpair(AF_LOCAL, SOCK_RAW|SOCK_CLOEXEC, 0) rc = %d",
		     rc);

		udev_monitor_fs_destroy(monitor);
		return rc;
	}

	monitor->sock = socket_fd[0];	// receiving end 
	monitor->sock_fs = socket_fd[1];	// sending end

	// unify inotify and sock behind epoll_fd, set to poll for reading
	ev.events = EPOLLIN;
	ev.data.fd = monitor->inotify_fd;
	rc = epoll_ctl(monitor->epoll_fd, EPOLL_CTL_ADD, monitor->inotify_fd,
		       &ev);
	if (rc != 0) {

		rc = -errno;
		log_error("epoll_ctl(%d on inotify_fd %d) rc = %d",
			  monitor->epoll_fd, monitor->inotify_fd, rc);

		udev_monitor_fs_shutdown(monitor);
		return rc;
	}

	ev.data.fd = monitor->sock;
	rc = epoll_ctl(monitor->epoll_fd, EPOLL_CTL_ADD, monitor->sock, &ev);
	if (rc != 0) {

		rc = -errno;
		log_error("epoll_ctl(%d on inotify_fd %d) rc = %d",
			  monitor->sock, monitor->sock, rc);

		udev_monitor_fs_shutdown(monitor);
		return rc;
	}

	monitor->pid = getpid();

	return rc;
}

// shut down a monitor's filesystem-specific state 
// not much we can do if any shutdown step fails, so try them all
int udev_monitor_fs_shutdown(struct udev_monitor *monitor)
{

	int rc = 0;

	// stop tracking this monitor
	udev_monitor_unregister(monitor);

	if (monitor->sock >= 0) {
		rc = shutdown(monitor->sock, SHUT_RDWR);
		if (rc < 0) {
			rc = -errno;
			log_error("shutdown(socket %d) rc = %d", monitor->sock,
				  rc);
		}
	}

	if (monitor->sock_fs >= 0) {
		rc = shutdown(monitor->sock_fs, SHUT_RDWR);
		if (rc < 0) {
			rc = -errno;
			log_error("shutdown(socket %d) rc = %d",
				  monitor->sock_fs, rc);
		}
	}

	if (monitor->sock >= 0) {
		rc = close(monitor->sock);
		if (rc < 0) {
			rc = -errno;
			log_error("close(socket %d) rc = %d", monitor->sock,
				  rc);
		} else {
			monitor->sock = -1;
		}
	}

	if (monitor->sock_fs >= 0) {
		rc = close(monitor->sock_fs);
		if (rc < 0) {
			rc = -errno;
			log_error("close(socket %d) rc = %d", monitor->sock_fs,
				  rc);
		} else {
			monitor->sock_fs = -1;
		}
	}

	if (monitor->epoll_fd >= 0) {
		rc = close(monitor->epoll_fd);
		if (rc < 0) {
			rc = -errno;
			log_error("close(epoll_fd %d) rc = %d",
				  monitor->epoll_fd, rc);
		} else {
			monitor->epoll_fd = -1;
		}
	}

	if (monitor->events_wd >= 0) {

		if (monitor->inotify_fd >= 0) {
			rc = inotify_rm_watch(monitor->inotify_fd,
					      monitor->events_wd);
			if (rc < 0) {
				rc = -errno;
				log_error("close(events_wd %d) rc = %d",
					  monitor->events_wd, rc);
			} else {
				monitor->events_wd = -1;
			}
		}
	}
	if (monitor->inotify_fd >= 0) {
		rc = close(monitor->inotify_fd);
		if (rc < 0) {
			rc = -errno;
			log_error("close(inotify_fd %d) rc = %d",
				  monitor->inotify_fd, rc);
		} else {
			monitor->inotify_fd = -1;
		}
	}

	return rc;
}

// blow away all local filesystem state for a monitor
int udev_monitor_fs_destroy(struct udev_monitor *monitor)
{

	char pathbuf[PATH_MAX + 1];
	int dirfd = 0;
	int rc = 0;
	DIR *dirh = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	bool can_rmdir = true;

	// stop listening
	udev_monitor_fs_shutdown(monitor);

	// remove events dir contents 
	dirfd = open(monitor->events_dir, O_DIRECTORY | O_CLOEXEC);
	if (dirfd < 0) {

		rc = -errno;
		log_error("open('%s') rc = %d", monitor->events_dir, rc);
		return rc;
	}

	dirh = fdopendir(dirfd);
	if (dirh == NULL) {

		// OOM
		rc = -errno;
		close(dirfd);
		return rc;
	}

	do {

		// next entry 
		rc = readdir_r(dirh, &entry, &result);
		if (rc != 0) {

			// I/O error 
			log_error("readdir_r('%s') rc = %d",
				  monitor->events_dir, rc);
			break;
		}
		// skip . and ..
		if (strcmp(entry.d_name, ".") == 0
		    || strcmp(entry.d_name, "..") == 0) {
			continue;
		}
		// generate full path 
		memset(pathbuf, 0, PATH_MAX + 1);

		snprintf(pathbuf, PATH_MAX, "%s/%s", monitor->events_dir,
			 entry.d_name);

		// optimistically remove
		if (entry.d_type == DT_DIR) {
			rc = rmdir(pathbuf);
		} else {
			rc = unlink(pathbuf);
		}

		if (rc != 0) {

			rc = -errno;
			log_error("remove '%s' rc = %d", pathbuf, rc);

			can_rmdir = false;
			rc = 0;
		}

	} while (result != NULL);

	// NOTE: closes dirfd
	closedir(dirh);

	if (can_rmdir) {
		rc = rmdir(monitor->events_dir);
		if (rc != 0) {

			rc = -errno;
			log_error("rmdir('%s') rc = %d\n", monitor->events_dir,
				  rc);
		}
	} else {
		// let the caller know...
		rc = -ENOTEMPTY;
	}

	return rc;
}

// thread-safe and async-safe int to string for base 10
void itoa10_safe(int val, char *str)
{

	int rc = 0;
	int i = 0;
	int len = 0;
	int j = 0;
	bool neg = false;

	// sign check
	if (val < 0) {
		val = -val;
		neg = true;
	}
	// consume, lowest-order to highest-order
	if (val == 0) {
		str[i] = '0';
		i++;
	} else {
		while (val > 0) {

			int r = val % 10;

			str[i] = '0' + r;
			i++;

			val /= 10;
		}
	}

	if (neg) {

		str[i] = '-';
		i++;
	}

	len = i;
	i--;

	// reverse order to get the number 
	while (j < i) {

		char tmp = *(str + i);
		*(str + i) = *(str + j);
		*(str + j) = tmp;

		j++;
		i--;
	}

	str[len] = '\0';
}

// path to a named event for this process to consume
// pathbuf must have at least PATH_MAX bytes
// NOTE: must be async-safe, since it's used in a pthread_atfork() callback
static int udev_monitor_fs_events_path(char const *name, char *pathbuf,
				       int nonce)
{

	// do the equivalent of:
	//    snprintf( pathbuf, PATH_MAX, UDEV_FS_EVENTS_DIR "/libudev-%d-%d/%s", getpid(), nonce, name );

	char pidbuf[10];
	pidbuf[9] = 0;

	char nonce_buf[50];
	nonce_buf[49] = 0;

	pid_t pid = getpid();

	itoa10_safe(pid, pidbuf);
	itoa10_safe(nonce, nonce_buf);

	size_t pidbuf_len = strnlen(pidbuf, 10);
	size_t nonce_buf_len = strnlen(nonce_buf, 50);
	size_t prefix_len = strlen(UDEV_FS_EVENTS_DIR);
	size_t dirname_prefix_len = strlen("/libudev-");
	int off = 0;

	memcpy(pathbuf, UDEV_FS_EVENTS_DIR, prefix_len);
	off += prefix_len;

	memcpy(pathbuf + off, "/libudev-", dirname_prefix_len);
	off += dirname_prefix_len;

	memcpy(pathbuf + off, pidbuf, pidbuf_len);
	off += pidbuf_len;

	memcpy(pathbuf + off, "-", 1);
	off += 1;

	memcpy(pathbuf + off, nonce_buf, nonce_buf_len);
	off += nonce_buf_len;

	pathbuf[off] = '/';
	off++;

	memcpy(pathbuf + off, name, strlen(name));
	off += strlen(name);

	pathbuf[off] = '\0';

	return off + strlen(name);
}

// send the contents of a file containing a serialized packet to the libudev client:
// * read the contents 
// * send it along to the receiving struct udev_monitor
// NOTE: The file format is expected to be the same as a uevent packet:
//       * all newlines (\n) will be converted to null (\0), since that's how
//         the kernel sends it.
//       * the buffer is expected to be at most 8192 bytes long.
// return 0 on success 
// return -errno on failure 
// return -EMSGSIZE if the file is too big 
// return -EBADMSG if the file is invalid
// return -EAGAIN if we'd block on send
// TODO: can we use sendfile(2)?
static int udev_monitor_fs_push_event(int fd, struct udev_monitor *monitor)
{

	int rc = 0;
	off_t len = 0;
	size_t hdrlen = 0;
	char buf[8192];
	struct stat sb;
	struct msghdr msg;
	struct iovec iov;
	struct udev_device *dev = NULL;
	size_t i = 0;

	memset(&msg, 0, sizeof(struct msghdr));
	memset(&iov, 0, sizeof(struct iovec));

	// first, get the size 
	rc = fstat(fd, &sb);
	if (rc < 0) {

		rc = -errno;
		log_error("fstat(%d) rc = %d", fd, rc);
		return rc;
	}

	if (sb.st_size >= 8192) {

		rc = -EMSGSIZE;
		return rc;
	}

	rc = udev_read_uninterrupted(fd, buf, sb.st_size);
	if (rc < 0) {

		log_error("udev_read_uninterrupted(%d) rc = %d", fd, rc);
		return rc;
	}
	// replace all '\n' with '\0', in case the caller wrote 
	// the file line by line.
	for (i = 0; i < sb.st_size; i++) {

		if (buf[i] == '\n') {
			buf[i] = '\0';
		}
	}

	// should be a uevent packet, and should start with [add|change|move|remove]@[devpath]\0
	hdrlen = strnlen(buf, sb.st_size) + 1;
	if (hdrlen < sizeof("a@/d") || hdrlen >= sb.st_size) {

		log_error
		    ("invalid message header: length = %zu, message length = %zd",
		     hdrlen, sb.st_size);
		return -EBADMSG;
	}

	if (strstr(buf, "@/") == NULL) {

		// invalid header 
		log_error("%s",
			  "invalid message header: missing '@' directive");
		return -EBADMSG;
	}
	// make a udev device 
	dev =
	    udev_device_new_from_nulstr(monitor->udev, &buf[hdrlen],
					sb.st_size - hdrlen);
	if (dev == NULL) {

		rc = -errno;
		log_error("udev_device_new_from_nulstr() rc = %d", rc);

		return rc;
	}
	// send it along 
	// TODO: sendfile(2)?
	rc = udev_monitor_send_device(monitor, NULL, dev);
	if (rc < 0) {

		log_error("udev_monitor_send_device rc = %d", rc);
	} else {

		rc = 0;
	}

	udev_device_unref(dev);
	return rc;
}

// reset the oneshot inotify watch, so it will trip on the next create.
// consume pending events, if there are any, and re-watch the directory.
// if the pid has changed since last time, watch the new directory.
// return 0 on success
// return negative on error (errno)
static int udev_monitor_fs_watch_reset(struct udev_monitor *monitor)
{

	int rc = 0;
	bool inotify_triggerred = false;	// was inotify triggerred?
	char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));	// see inotify(7)
	struct pollfd pfd[1];

	pfd[0].fd = monitor->inotify_fd;
	pfd[0].events = POLLIN;

	// reset the watch by consuming all its events (should be at most one)
	while (1) {

		// do we have data?
		rc = poll(pfd, 1, 0);
		if (rc <= 0) {

			// out of data, or error 
			if (rc < 0) {

				rc = -errno;
				if (rc == -EINTR) {

					// shouldn't really happen since the timeout for poll is zero,
					// but you never know...
					continue;
				}

				log_error("poll(%d) rc = %d\n",
					  monitor->inotify_fd, rc);
				rc = 0;
			}

			break;
		}
		// at least one event remaining
		// consume it
		rc = read(monitor->inotify_fd, buf, 4096);
		if (rc == 0) {
			break;
		}

		if (rc < 0) {

			rc = -errno;

			if (rc == -EINTR) {
				continue;
			} else if (rc == -EAGAIN || rc == -EWOULDBLOCK) {
				rc = 0;
				break;
			}
		}
		// got one event
		inotify_triggerred = true;
	}

	// has the PID changed?
	// need to regenerate events path 
	if (getpid() != monitor->pid) {

		log_trace("Switch PID from %d to %d", monitor->pid, getpid());

		udev_monitor_fs_events_path("", monitor->events_dir,
					    monitor->slot);

		rc = mkdir(monitor->events_dir, 0700);
		if (rc != 0) {

			rc = -errno;
			if (rc != -EEXIST) {

				log_error("mkdir('%s') rc = %d\n",
					  monitor->events_dir, rc);
				return rc;
			} else {
				rc = 0;
			}
		}

		monitor->events_wd =
		    inotify_add_watch(monitor->inotify_fd, monitor->events_dir,
				      UDEV_FS_WATCH_DIR_FLAGS);
		if (monitor->events_wd < 0) {

			rc = -errno;

			log_error("inotify_add_watch('%s') rc = %d\n",
				  monitor->events_dir, rc);
			return rc;
		}

		monitor->pid = getpid();

		// TODO: what about events that the child was supposed to receive?
		// the parent forks, receives one or more events, and the child wakes up, and will miss them 
		// if we only do the above.
		// we need to (try to) consume them here
	}

	rc = inotify_add_watch(monitor->inotify_fd, monitor->events_dir,
			       UDEV_FS_WATCH_DIR_FLAGS);
	if (rc < 0) {

		rc = -errno;
		log_error("inotify_add_watch(%d) rc = %d", monitor->inotify_fd,
			  rc);
	}

	return rc;
}

// scandir: skip . and ..
static int udev_monitor_fs_scandir_filter(const struct dirent *dent)
{

	if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
		return 0;
	} else {
		return 1;
	}
}

// push as many available events from our filesystem buffer of events off to the monitor's socketpair.
// the order is determined lexicographically (i.e. we assume that whatever is writing events is naming them in 
// a monotonically increasing order, such as e.g. the SEQNUM field)
// return 0 on success
// return -ENODATA if there are no events
// return -EAGAIN if there are events, but we couldn't push any
// return -errno if we can't re-watch the directory
// NOTE: this method should only be used if the underlying filesystem holding the events can't help us preserve the order.
// NOTE: not thread-safe
int udev_monitor_fs_push_events(struct udev_monitor *monitor)
{

	char pathbuf[PATH_MAX + 1];
	int dirfd = -1;
	int fd = -1;
	int rc = 0;
	int num_events = 0;
	int num_valid_events = 0;
	int num_sent = 0;
	struct dirent **events = NULL;	// names of events to buffer
	int i = 0;

	// reset the watch on this directory, and ensure we're watching the right one.
	rc = udev_monitor_fs_watch_reset(monitor);
	if (rc < 0) {

		log_error("Failed to re-watch '%s', rc = %d",
			  monitor->events_dir, rc);
		goto udev_monitor_fs_push_events_cleanup;
	}
	// find new events... 
	dirfd = open(monitor->events_dir, O_DIRECTORY | O_CLOEXEC);
	if (dirfd < 0) {

		rc = -errno;
		log_error("open('%s') rc = %d", monitor->events_dir, rc);
		goto udev_monitor_fs_push_events_cleanup;
	}

	num_events =
	    scandirat(dirfd, ".", &events, udev_monitor_fs_scandir_filter,
		      alphasort);

	if (num_events < 0) {

		rc = -errno;
		log_error("scandir('%s') rc = %d", monitor->events_dir, rc);
		goto udev_monitor_fs_push_events_cleanup;
	}

	if (num_events == 0) {

		// got nothing
		rc = -ENODATA;
		goto udev_monitor_fs_push_events_cleanup;
	}

	num_valid_events = num_events;

	// send them all off!
	for (i = 0; i < num_events; i++) {

		snprintf(pathbuf, PATH_MAX, "%s/%s", monitor->events_dir,
			 events[i]->d_name);

		fd = open(pathbuf, O_RDONLY | O_CLOEXEC);
		if (fd < 0) {

			rc = -errno;
			log_error("cannot open event: open('%s') rc = %d",
				  pathbuf, rc);

			// we consider it more important to preserve order and drop events 
			// than to try to resend later.
			unlink(pathbuf);
		} else {

			// propagate to the monitor's socket
			rc = udev_monitor_fs_push_event(fd, monitor);

			// garbage-collect
			close(fd);
			unlink(pathbuf);

			if (rc == -EBADMSG || rc == -EMSGSIZE) {

				// invalid message anyway
				rc = 0;
				num_valid_events--;
				continue;
			}

			else if (rc < 0) {

				if (rc != -EAGAIN) {

					// socket-level error
					log_error
					    ("failed to push event '%s', rc = %d",
					     pathbuf, rc);
					break;
				} else {

					// sent as many as we could 
					rc = 0;
					break;
				}
			} else if (rc == 0) {

				num_sent++;
			}
		}
	}

 udev_monitor_fs_push_events_cleanup:

	if (dirfd >= 0) {
		close(dirfd);
	}

	if (events != NULL) {
		for (i = 0; i < num_events; i++) {

			if (events[i] != NULL) {
				free(events[i]);
				events[i] = NULL;
			}
		}

		free(events);
	}

	if (num_sent == 0 && num_valid_events > 0) {

		// there are pending events, but we couldn't push any.
		rc = -EAGAIN;
	}

	return rc;
}

#ifdef TEST

#include "libudev.h"

int main(int argc, char **argv)
{

	int rc = 0;
	char pathbuf[PATH_MAX + 1];
	struct udev *udev_client = NULL;
	struct udev_monitor *monitor = NULL;
	int monitor_fd = 0;
	struct udev_device *dev = NULL;
	struct pollfd pfd[1];
	int num_events = INT_MAX;
	int num_forks = 0;

	// usage: $0 [num events to process [num times to fork]]
	if (argc > 1) {
		char *tmp = NULL;
		num_events = (int)strtol(argv[1], &tmp, 10);
		if (tmp == argv[1] || *tmp != '\0') {
			fprintf(stderr,
				"Usage: %s [number of events to process [number of times to fork]]\n",
				argv[0]);
			exit(1);
		}

		if (argc > 2) {

			num_forks = (int)strtol(argv[2], &tmp, 10);
			if (tmp == argv[2] || *tmp != '\0') {
				fprintf(stderr,
					"Usage: %s [number of events to process [number of times to fork]]\n",
					argv[0]);
				exit(1);
			}
		}
	}
	// make sure events dir exists 
	log_trace("events directory '%s'", UDEV_FS_EVENTS_DIR);

	rc = mkdir(UDEV_FS_EVENTS_DIR, 0700);
	if (rc != 0) {

		rc = -errno;
		if (rc != -EEXIST) {
			log_error("mkdir('%s') rc = %d", UDEV_FS_EVENTS_DIR,
				  rc);
			exit(1);
		}
	}

	udev_monitor_fs_events_path("", pathbuf, 0);

	printf("Watching '%s'\n", pathbuf);

	udev_client = udev_new();
	if (udev_client == NULL) {

		// OOM
		exit(2);
	}

	monitor = udev_monitor_new_from_netlink(udev_client, "udev");
	if (monitor == NULL) {

		// OOM or error
		udev_unref(udev_client);
		exit(2);
	}

	printf("Press Ctrl-C to quit\n");

	monitor_fd = udev_monitor_get_fd(monitor);
	if (monitor_fd < 0) {

		rc = -errno;
		log_error("udev_monitor_get_fd rc = %d\n", rc);
		exit(3);
	}

	pfd[0].fd = monitor_fd;
	pfd[0].events = POLLIN;

	while (num_events > 0) {

		// wait for the next device 
		rc = poll(pfd, 1, -1);
		if (rc < 0) {

			log_error("poll(%d) rc = %d\n", monitor_fd, rc);
			break;
		}
		// get devices 
		while (num_events > 0) {

			dev = udev_monitor_receive_device(monitor);
			if (dev == NULL) {
				break;
			}

			int pid = getpid();
			struct udev_list_entry *list_entry = NULL;

			printf("[%d] [%d] ACTION:     '%s'\n", pid, num_events,
			       udev_device_get_action(dev));
			printf("[%d] [%d] SEQNUM:      %llu\n", pid, num_events,
			       udev_device_get_seqnum(dev));
			printf("[%d] [%d] USEC:        %llu\n", pid, num_events,
			       udev_device_get_usec_since_initialized(dev));
			printf("[%d] [%d] DEVNODE:    '%s'\n", pid, num_events,
			       udev_device_get_devnode(dev));
			printf("[%d] [%d] DEVPATH:    '%s'\n", pid, num_events,
			       udev_device_get_devpath(dev));
			printf("[%d] [%d] SYSNAME:    '%s'\n", pid, num_events,
			       udev_device_get_sysname(dev));
			printf("[%d] [%d] SYSPATH:    '%s'\n", pid, num_events,
			       udev_device_get_syspath(dev));
			printf("[%d] [%d] SUBSYSTEM:  '%s'\n", pid, num_events,
			       udev_device_get_subsystem(dev));
			printf("[%d] [%d] DEVTYPE:    '%s'\n", pid, num_events,
			       udev_device_get_devtype(dev));
			printf("[%d] [%d] SYSNUM:     '%s'\n", pid, num_events,
			       udev_device_get_sysnum(dev));
			printf("[%d] [%d] DRIVER:     '%s'\n", pid, num_events,
			       udev_device_get_driver(dev));
			printf("[%d] [%d] DEVNUM:      %d:%d\n", pid,
			       num_events, major(udev_device_get_devnum(dev)),
			       minor(udev_device_get_devnum(dev)));
			printf("[%d] [%d] IFINDEX:    '%s'\n", pid, num_events,
			       udev_device_get_property_value(dev, "IFINDEX"));
			printf("[%d] [%d] DEVMODE:    '%s'\n", pid, num_events,
			       udev_device_get_property_value(dev, "DEVMODE"));
			printf("[%d] [%d] DEVUID:     '%s'\n", pid, num_events,
			       udev_device_get_property_value(dev, "DEVUID"));
			printf("[%d] [%d] DEVGID:     '%s'\n", pid, num_events,
			       udev_device_get_property_value(dev, "DEVGID"));

			list_entry = udev_device_get_devlinks_list_entry(dev);
			udev_list_entry_foreach(list_entry,
						udev_list_entry_get_next
						(list_entry)) {

				printf("[%d] [%d] devlink:    '%s'\n", pid,
				       num_events,
				       udev_list_entry_get_name(list_entry));
			}

			list_entry = udev_device_get_properties_list_entry(dev);
			udev_list_entry_foreach(list_entry,
						udev_list_entry_get_next
						(list_entry)) {

				printf("[%d] [%d] property:   '%s' = '%s'\n",
				       pid, num_events,
				       udev_list_entry_get_name(list_entry),
				       udev_list_entry_get_value(list_entry));
			}

			list_entry = udev_device_get_tags_list_entry(dev);
			udev_list_entry_foreach(list_entry,
						udev_list_entry_get_next
						(list_entry)) {

				printf("[%d] [%d] tag:        '%s'\n", pid,
				       num_events,
				       udev_list_entry_get_name(list_entry));
			}

			list_entry = udev_device_get_sysattr_list_entry(dev);
			udev_list_entry_foreach(list_entry,
						udev_list_entry_get_next
						(list_entry)) {

				printf("[%d] [%d] sysattr:    '%s'\n", pid,
				       num_events,
				       udev_list_entry_get_name(list_entry));
			}

			printf("\n");

			udev_device_unref(dev);

			num_events--;
		}

		// do our forks
		if (num_forks > 0) {

			num_forks--;

			int pid = fork();
			if (pid < 0) {

				rc = -errno;
				fprintf(stderr, "fork: %s\n", strerror(-rc));
				break;
			} else if (pid == 0) {

				printf("[%d]\n", getpid());
			}
		}
	}

	udev_monitor_fs_destroy(monitor);

	exit(0);
}

#endif
