vdev: a virtual device manager and filesystem
=============================================

**This system is a work-in-progress.  If you would like to help, please see the [Issue Tracker](https://github.com/jcnelson/vdev/issues)**.

Vdev is a userspace device manager and filesystem that exposes attached devices as device files for UNIX-like operating systems.  It differs from FreeBSD's devfs or Linux's udev, eudev, and mdev in that it offers an optional filesystem interface that implements a *per-process view of /dev* in a portable manner, providing the necessary functionality for the host administrator to control device node visibility and access based on arbitrary criteria (e.g. process UID, process session, process seat, etc.).

More information is available in the [design document](http://judecnelson.blogspot.com/2015/01/introducing-vdev.html).

Project Goals
-------------
* **Portable Architecture**.  Vdev is designed to be portable across (modern) *nix.  It can interface with both the kernel and synthetic device event sources, such as another vdev instance, or an existing /dev tree.  It can be statically linked and can compile with multiple libc's.
* **Event-driven**.  Vdev's core logic is built around reacting to events from its back-end event source.  It creates and removes device files and metadata in response to devices being plugged in or unplugged.
* **Scriptable**.  Vdev aims to keep device management policy and mechanisms as separate as possible.  It offers an easy-to-understand programming model for matching a device event to a sequence administrator-defined programs to run.
* **Advanced Access Control**.  Vdev comes with an optional userspace filesystem that lets the administrator control how individual processes see the files under /dev.  The criteria include not only the process's effective UID and GID, but also the process image's inode number, absolute path, binary checksum, sets of open files, seat assignment, and so on.  *Any* process information can be used to control access.
* **Container Friendly**.  Vdev can run in containers and chroots, and offers the administrator an easy-to-understand way to restrict, reorder, and rewrite the device events the contained vdevd will observe.  Importantly, the Linux port of vdev does *not* rely on netlink sockets to propagate events to client programs.
* **Backwards Compatible**.  Vdev works with existing device management frameworks on the host OS.  The Linux port in particular comes with scripts to maintain a /dev tree that is backwards-compatible with udev, and comes with a "libudev-compat" library that is ABI-compatible with libudev 219.  The Linux port's udev-compatibility scripts generate and propagate device events and maintain device information in /run/udev, and libudev-compat allows libudev-dependent programs to continue working without modification.

Project Non-Goals
-----------------
* **Init system integration**.  Vdev is meant to run with full functionality regardless of which init system, daemon manager, service manager, plumbing layer, etc. the user runs, since they address orthogonal concerns.  It will coexist with and complement them, but neither forcibly replace them nor require them to be present.
* **New client libraries, language bindings, or D-bus APIs**.  Vdev will exprose all of the necessary state and device metadata via the /dev filesystem directly.  Any wrappers that present this information via higher-level APIs (such as libudev) will be supported only to provide backwards compatibility.
* **Devtmpfs dependency**.  The Linux port of vdev does *not* require devtmpfs.

Dependencies
-----------

There are two binaries in vdev:  the hotplug daemon vdevd, and the userspace filesystem vdevfs.  You can use one without the other.

To build vdevd, you'll need:
* libc
* libpthread

For vdevfs, you'll need all of the above, plus:
* libstdc++
* FUSE
* [libpstat](https://github.com/jcnelson/libpstat)
* [fskit](https://github.com/jcnelson/fskit)

Building
--------

To build and install everything, with default options:

    $ make
    $ sudo make install 

To build and install just vdevd, type:

    $ make -C vdevd OS=$OS_TYPE
    $ sudo make -C vdevd install

Substitute $OS_TYPE with:
* "LINUX" to build for Linux (the default value)
* "OPENBSD" to build for OpenBSD (coming soon)

$OS_TYPE defaults to "LINUX".

To build and install just vdevfs, type:

    $ make -C fs
    $ sudo make -C fs install

By default, vdevd is installed to /sbin/vdevd, vdevd's helper programs and hardware database are installed to /lib/vdev/, and vdevfs is installed to /usr/sbin/vdevfs.  You can override any of these directory choices at build-time by setting the "PREFIX=" variable on the command-line (e.g. `make -C vdevd PREFIX=/usr/local/`), and you can override the installation location by setting "DESTDIR=" at install-time (e.g. `sudo make -C vdevd install DESTDIR=/usr/local/`).

FAQ
---
* **Why another device manager?**  I want to control which processes have access to which device nodes based on criteria other than their user and group IDs.  For example, I want to filter access based on which login session the process belongs to, and I want to filter based on which container a process runs in.  Also, I want to have this functionality available on each of the *nix OSs I use on a (semi-)regular basis.  To the best of my knowledge, no device manager lets me do this (except perhaps Plan 9's per-process namespaces), so I wrote my own.
* **What is the license for vdev?**  Vdev code is available for use under the terms of either the [GPLv3+](https://github.com/jcnelson/vdev/blob/master/LICENSE.GPLv3%2B) or [ISC license](https://github.com/jcnelson/vdev/blob/master/LICENSE.ISC).  However, the Linux port of vdev ships with some optional Linux-specific [helper programs](https://github.com/jcnelson/vdev/tree/master/vdevd/helpers/LINUX) that were derived from udev.  They are available under the terms of the GPLv2 where noted in the source files.
* **How much time do you have to spend on this project?**  This is a side-project that's not directly related to my day job, so nights and weekends when I have cycles to spare.  However, I am very interested in the success of this project, so expect frequent updates nevertheless :)
* **Which Linux distributions are you targeting?**  vdev is distro-agnostic, but I'm developing and testing primarily for [Debian](http://www.debian.org) and [Devuan](http://devuan.org).
* **Which BSDs/other UNIXes are you planning to support?**  OpenBSD for now.  Well-written, suitably-licensed pull requests that port vdev to these or other UNIX-like OSs will be gratefully accepted.
* **Does this project have anything to do with systemd and udev merging?**  It's not strictly related, but the pending merger definitely motivated me to start this project sooner rather than later.  I hope vdev's existence will help alleviate the tension between the pro- and anti-systemd crowds:
  * Linux users who don't want to use systemd can run vdev in place of udev.
  * Linux Users who prefer some other device manager besides udev or vdev but need to run software that depends on libudev can use libudev-compat (vdev's udev-compatibility helper programs are easily portable to other device managers).
  * Linux developers who need fine-grained device access controls but don't want to couple their software to systemd will have vdevfs as a portable, easy-to-use alternative.
  * Linux users do not need to choose between systemd and vdevfs, since they can coexist.
  * The systemd developers can tightly couple udev to systemd-PID-1 if they want, since non-systemd users will have an alternative.
  * **Result:** Everyone wins.
