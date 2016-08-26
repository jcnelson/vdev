/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.GPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU General
   Public License, but WITHOUT ANY WARRANTY; without even the implied 
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   Alternatively, you are free to use this program under the terms of the 
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or 
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

// log a device event to each listening program in /dev/events:
// 0. read it from stdin; verify that it has a seqnum
// 1. put it into /dev/events/global/$SEQNUM
// 2. hard-link /dev/events/global/$SEQNUM to each /dev/events/*/$SEQNUM
// 3. unlink /dev/events/$SEQNUM

#include "common.h"

#define DEFAULT_DEV_EVENTS "/dev/events/global"

// path to device events directory (overrideable in command-line)
static char g_dev_events[PATH_MAX + 1];

// make a path to an event in the global queue 
// event_path must have at least PATH_MAX+1 bytes
int
make_event_path (uint64_t seqnum, char *event_path)
{

  memset (event_path, 0, PATH_MAX + 1);

  return snprintf (event_path, PATH_MAX, "%s/%" PRIu64, g_dev_events, seqnum);
}

// open the event file, exclusively, for writing.
// make sure that only the UID/GID of this process (i.e. root) can access it.
int
open_event (char *event_path)
{

  int fd = 0;

  fd = open (event_path, O_WRONLY | O_EXCL | O_CREAT, 0600);
  if (fd < 0)
    {

      fd = -errno;
      log_error ("open('%s') rc = %d\n", event_path, fd);
      return fd;
    }

  return fd;
}

// clear an event 
// return 0 on success
// return -errno (from unlink(2)) on failure
int
clear_event (char *event_path)
{

  int rc = unlink (event_path);
  if (rc < 0)
    {

      rc = -errno;
      log_error ("unlink('%s') rc = %d\n", event_path, rc);
    }

  return rc;
}

// link all events to all directories that are siblings to the parent directory of the event path.
// try to do so even if we fail to link in some cases 
// return 0 on success
// return -errno if at least one failed
int
multicast_event (uint64_t seqnum, char *event_path)
{

  int rc = 0;
  DIR *dirh = NULL;
  struct dirent entry;
  struct dirent *result = NULL;
  char pathbuf[PATH_MAX + 1];

  char event_queues_dir[PATH_MAX + 1];
  char global_queue_name[NAME_MAX + 1];

  memset (event_queues_dir, 0, PATH_MAX + 1);
  memset (global_queue_name, 0, NAME_MAX + 1);

  // parent nmae of event_path...
  ssize_t i = strlen (event_path) - 1;
  if (i < 0)
    {
      return -EINVAL;
    }
  // shouldn't end in '/', but you never know...
  while (i > 0 && event_path[i] == '/')
    {
      i--;
    }

  // skip event name
  while (i > 0 && event_path[i] != '/')
    {
      i--;
    }

  // skip '/' at the end of the parent name 
  while (i > 0 && event_path[i] == '/')
    {
      i--;
    }

  if (i == 0)
    {

      // event_path's parent is /
      strcpy (global_queue_name, ".");
      strcpy (event_queues_dir, "/");
    }
  else
    {

      ssize_t parent_end = i;

      while (i > 0 && event_path[i] != '/')
	{
	  i--;
	}

      strncpy (global_queue_name, &event_path[i + 1], parent_end - i);
      strncpy (event_queues_dir, event_path, i + 1);
    }

  dirh = opendir (event_queues_dir);
  if (dirh == NULL)
    {

      // OOM
      rc = -errno;
      return rc;
    }

  do
    {

      // next entry 
      rc = readdir_r (dirh, &entry, &result);
      if (rc != 0)
	{

	  // I/O error 
	  log_error ("readdir_r('%s') rc = %d", event_queues_dir, rc);
	  break;
	}
      // skip . and ..
      if (strcmp (entry.d_name, ".") == 0 || strcmp (entry.d_name, "..") == 0)
	{
	  continue;
	}
      // skip non-directories 
      if (entry.d_type != DT_DIR)
	{
	  continue;
	}
      // skip global queue 
      if (strcmp (global_queue_name, entry.d_name) == 0)
	{
	  continue;
	}
      // link to this directory 
      snprintf (pathbuf, PATH_MAX, "%s/%s/%" PRIu64, event_queues_dir,
		entry.d_name, seqnum);

      rc = link (event_path, pathbuf);
      if (rc != 0)
	{

	  rc = errno;
	  log_error ("link('%s', '%s'): %s", event_path, pathbuf,
		     strerror (-rc));
	  rc = 0;
	}

    }
  while (result != NULL);

  closedir (dirh);

  return rc;
}

// print usage statement
int
usage (char const *progname)
{

  int i = 0;
  char const *usage_text[] = {
    "Usage: ", progname,
    " [-v] [-n SEQNUM] [-s SOURCE-QUEUE] [-t TARGET-QUEUES]\n",
    "Options:\n",
    "   -n SEQNUM\n",
    "                  Event sequence number.  Must be\n",
    "                  unique across all pending events.\n",
    "\n",
    "   -s SOURCE-QUEUE\n",
    "                  Path to the source event queue.  An\n",
    "                  event will be added as a child of this\n",
    "                  directory, and linked into all directories\n",
    "                  in TARGET-QUEUES.  The default is\n",
    "                  " DEFAULT_DEV_EVENTS, "\n",
    "\n",
    NULL
  };

  for (i = 0; usage_text[i] != NULL; i++)
    {
      fprintf (stderr, usage_text[i]);
    }

  return 0;
}

// print a verbose help statement 
int
help (char const *progname)
{

  usage (progname);

  int i = 0;
  char const *help_text[] = {
    "This program reads a device event from standard input, such that\n",
    "each line takes the form of KEY=VALUE.  Valid keys and values are:\n",
    "\n",
    "   DEVPATH=(/devices path) [REQUIRED]\n",
    "   SUBSYSTEM=(subsystem name) [REQUIRED]\n",
    "   SEQNUM=(kernel uevent sequence number) [REQUIRED if -n is not given]\n",
    "   DEVNAME=(/dev node path)\n",
    "   DEVLINKS=(space-separated list of symlinks)\n",
    "   TAGS=(colon-separated list of tags)\n",
    "   USEC_INITIALIZED=(microseconds since device initialization)\n",
    "   DRIVER=(name of device driver)\n",
    "   ACTION=(add, remove, change, move, etc.)\n",
    "   MAJOR=(major device number)\n",
    "   MINOR=(minor device number)\n",
    "   DEVPATH_OLD=(old device path (on move)\n",
    "   IFINDEX=(device interface index, e.g. for USB and network interfaces)\n",
    "   DEVMODE=(device node permission bits)\n",
    "   DEVUID=(device node UID)\n",
    "   DEVGID=(device node GID)\n",
    "\n",
    "Keys marked with [REQUIRED] must be present.\n",
    "NOTE: events are limited to 8192 bytes.\n",
    NULL
  };

  for (i = 0; help_text[i] != NULL; i++)
    {
      fprintf (stderr, help_text[i]);
    }

  return 0;
}

// read, but mask EINTR
// return number of bytes read on success 
// return -errno on I/O error
// NOTE: must be async-safe!
ssize_t
read_uninterrupted (int fd, char *buf, size_t len)
{

  ssize_t num_read = 0;

  if (buf == NULL)
    {
      return -EINVAL;
    }

  while ((unsigned) num_read < len)
    {
      ssize_t nr = read (fd, buf + num_read, len - num_read);
      if (nr < 0)
	{

	  int errsv = -errno;
	  if (errsv == -EINTR)
	    {
	      continue;
	    }

	  return errsv;
	}
      if (nr == 0)
	{
	  break;
	}

      num_read += nr;
    }

  return num_read;
}

// write, but mask EINTR
// return number of bytes written on success 
// return -errno on I/O error
ssize_t
write_uninterrupted (int fd, char const *buf, size_t len)
{

  ssize_t num_written = 0;

  if (buf == NULL)
    {
      return -EINVAL;
    }

  while ((unsigned) num_written < len)
    {
      ssize_t nw = write (fd, buf + num_written, len - num_written);
      if (nw < 0)
	{

	  int errsv = -errno;
	  if (errsv == -EINTR)
	    {
	      continue;
	    }

	  return errsv;
	}
      if (nw == 0)
	{
	  break;
	}

      num_written += nw;
    }

  return num_written;
}

// entry point
int
main (int argc, char **argv)
{

  int rc = 0;
  uint64_t seqnum = 0;
  int opt_index = 0;
  int c = 0;
  int fd = 0;
  char *tmp = NULL;
  char event_buf[8192];
  char event_buf_tmp[8292];	// has to hold event_buf plus an 8-byte sequence number
  char event_path[PATH_MAX + 1];

  bool have_seqnum = false;
  ssize_t nr = 0;
  char const *required_fields[] = {
    "\nSUBSYSTEM=",
    "\nDEVPATH=",
    NULL
  };

  char target_queues[PATH_MAX + 1];

  // default event queue
  memset (g_dev_events, 0, PATH_MAX + 1);
  memset (target_queues, 0, PATH_MAX + 1);
  memset (event_path, 0, PATH_MAX + 1);

  strcpy (g_dev_events, DEFAULT_DEV_EVENTS);

  static struct option opts[] = {
    {"source-queue", required_argument, 0, 's'},
    {"seqnum", required_argument, 0, 'n'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  char const *optstr = "n:s:h";

  while (rc == 0 && c != -1)
    {

      c = getopt_long (argc, argv, optstr, opts, &opt_index);
      if (c == -1)
	{

	  break;
	}

      switch (c)
	{

	case 's':
	  {

	    memset (g_dev_events, 0, PATH_MAX);
	    strncpy (g_dev_events, optarg, PATH_MAX);
	    break;
	  }

	case 'n':
	  {

	    seqnum = (uint64_t) strtoull (optarg, &tmp, 10);
	    if (seqnum == 0 && (tmp == optarg || *tmp != '\0'))
	      {

		usage (argv[0]);
		exit (1);
	      }

	    have_seqnum = true;
	    break;
	  }

	case 'h':
	  {

	    help (argv[0]);
	    exit (0);
	  }

	default:
	  {

	    fprintf (stderr,
		     "[ERROR] %s: Unrecognized option '%c'\n", argv[0], c);
	    usage (argv[0]);
	    exit (1);
	  }
	}
    }

  // get the event 
  memset (event_buf, 0, 8192);
  nr = read_uninterrupted (STDIN_FILENO, event_buf, 8192);

  if (nr <= 0)
    {

      rc = -errno;
      fprintf (stderr,
	       "[ERROR] %s: Failed to read event from stdin: %s\n",
	       argv[0], strerror (-rc));
      exit (1);
    }
  // simple sanity check for requirements 
  for (int i = 0; required_fields[i] != NULL; i++)
    {

      if (strstr (event_buf, required_fields[i]) == NULL)
	{

	  // head of line? with no leading '\n'?
	  if (strncmp
	      (event_buf, required_fields[i] + 1,
	       strlen (required_fields[i]) - 1) != 0)
	    {

	      fprintf (stderr,
		       "[ERROR] %s: Missing required field '%s'\n",
		       argv[0], required_fields[i] + 1);
	      fprintf (stderr,
		       "[ERROR] %s: Pass -h for a list of required fields\n",
		       argv[0]);
	      exit (1);
	    }
	}
    }

  // do we have a seqnum?
  if (!have_seqnum)
    {

      char *seqnum_str = strstr (event_buf, "SEQNUM=");

      // go find it in the device event
      if (seqnum_str == NULL)
	{

	  fprintf (stderr,
		   "[ERROR] %s: Missing SEQNUM.  Pass -n or include SEQNUM= in the input.\n",
		   argv[0]);
	  exit (1);
	}
      // is it a valid seqnum?
      seqnum =
	(uint64_t) strtoull (seqnum_str + strlen ("SEQNUM="), &tmp, 10);
      if (seqnum == 0
	  && (tmp == seqnum_str + strlen ("SEQNUM=") || *tmp != '\n'))
	{

	  // invalid seqnum 
	  fprintf (stderr,
		   "[ERROR] %s: Invalid SEQNUM.  Pass -n or include a valid SEQNUM in the input.\n",
		   argv[0]);
	  exit (1);
	}
    }
  // send it off!
  make_event_path (seqnum, event_path);

  fd = open_event (event_path);
  if (fd < 0)
    {

      fprintf (stderr, "[ERROR] %s: Failed to open '%s': %s\n",
	       argv[0], event_path, strerror (-fd));
      exit (1);
    }

  rc = write_uninterrupted (fd, event_buf, nr);
  if (rc < 0)
    {

      fprintf (stderr, "[ERROR] %s: Failed to write '%s': %s\n",
	       argv[0], event_path, strerror (-fd));

      clear_event (event_buf);
      close (fd);
      exit (1);
    }
  // propagate....
  rc = multicast_event (seqnum, event_path);
  if (rc < 0)
    {

      fprintf (stderr, "[ERROR] %s: Failed to multicast '%s': %s\n",
	       argv[0], event_path, strerror (-rc));

      clear_event (event_buf);
      close (fd);
      exit (1);
    }
  // done!
  clear_event (event_path);
  close (fd);

  return 0;
}
