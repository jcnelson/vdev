vdev: a virtual device filesystem
=================================

**This system is a work-in-progress.  If you would like to help, please see the [Issue Tracker](https://github.com/jcnelson/vdev/issues)**.

vdev is a userspace virtual filesystem that exposes attached devices as device nodes for UNIX-like operating systems.  It differs from FreeBSD's devfs or Linux's udev, eudev, and mdev in that it offers *per-process access control* in a portable manner, providing the necessary functionality for the host administrator to control device node access based on arbitrary process groupings (e.g. per-user, per-session, per-seat, etc.).

Project Goals
-------------
* **Portable Architecture**.  Unlike devfs or udev, vdev is designed to be portable across (modern) *nix.  As such, all OS-specific functionality is abstracted out of the core logic, and the core logic makes minimal assumptions about the underlying OS's device API.  In addition, the architecture supports multiple hardware event sources, including non-OS sources (such as another vdev instance, or modifications to an existing /dev tree).
* **Event-driven**.  Like devfs and udev, vdev leverages the underlying OS's hardware event-notification APIs.  Device nodes appear and disappear as the devices themselves become available or unavailable.  As with other device managers, users can define actions for vdev to take when a device's state changes.
* **Advanced Access Control**.  Unlike existing systems, vdev's ACL rules can filter access to device nodes based on which process is asking, in addition to filtering on user ID, group ID, and mode.  Moreover, the set of PIDs for which an ACL rule applies can be determined at runtime using an admin-supplied external program.  This obviates the need for setuid/setgid to allow otherwise-unprivileged binaries to access device nodes, and decouples device access control from orthogonal administrative tasks like session management and seat management.
* **Container-Friendly**.  vdev runs without modification within containers and jails.  Like devfs, it exposes only the devices the root context wishes the contained process to see.  In addition, it is possible to run multiple instances of vdev concurrently, alongside devfs or udev.

Project Non-Goals
-----------------
* **Init system integration**.  vdev is meant to run with full functionality regardless of which init system, daemon manager, service manager, plumbing layer, etc. the user runs, since they address orthogonal concerns.  It will coexist with and complement them, but neither forcibly replace them nor require them to be present.

Dependencies
------------
* libc
* libstdc++
* libpthread
* FUSE
* [fskit](https://github.com/jcnelson/fskit)
* [libpstat](https://github.com/jcnelson/libpstat)

Building
--------

To build, type:

    $ make OS=$OS_TYPE

Substitute $OS_TYPE with:
* "LINUX" to build for Linux
* "FREEBSD" to build for FreeBSD (coming soon)
* "OPENBSD" to build for OpenBSD (coming soon)

$OS_TYPE defaults to "LINUX".

To install, type:

    $ make install

The binaries will be installed by default to /usr/local/bin.

FAQ
---
* **Why another device manager?**  I want to control which processes have access to which device nodes based on criteria other than their user and group IDs.  For example, I want to filter access based on which login session the process belongs to, and I want to filter based on which container a process runs in.  Also, I want to have this functionality available on each of the *nix OSs I use on a (semi-)regular basis.  To the best of my knowledge, no device manager exists that lets me do this, so I wrote my own.
* **What is the license for vdev?**  Currently, GPLv3+.  I plan to dual-license or re-license under the ISC to reach a wider audience, but I'm still in the process of figuring out the right way forward.  Pull requests should be ISC-licensed if they are to be merged into the master branch.
* **Will vdev have a DBus API, or a C library?**  These are not necessary.  Since vdev is a filesystem, it exposes its API as a directory hierarchy instead.
* **How much time do you have to spend on this project?**  This is a side-project that's not directly related to my day job, so nights and weekends when I have cycles to spare.  Fortunately, most of the infrastructure is already in place.
* **Which Linux distributions are you targeting?**  vdev is distro-agnostic, but I'm developing and testing primarily for [Debian](http://www.debian.org) and [Devuan](http://devuan.org).
* **Which BSDs/other UNIXes are you planning to support?**  OpenBSD and FreeBSD for now.  Well-written, suitably-licensed pull requests that port vdev to these or other UNIX-like OSs will be gratefully accepted.
* **Does this project have anything to do with systemd and udev merging?**  While not strictly related, I hope vdev's existence will help alleviate the tension between the pro- and anti-systemd crowds.  Linux users who don't want to use systemd won't have to foresake dynamic device management, and Linux developers who need fine-grained device access control won't have to couple their applications to systemd to get it.  Everyone wins.
