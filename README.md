vdev: a virtual device filesystem
=================================

**This system is a work-in-progress.  If you would like to help, please see the [Issue Tracker](https://github.com/jcnelson/vdev/issues)**.

vdev is a userspace virtual filesystem that exposes attached devices as device nodes.  It differs from FreeBSD's devfs or Linux's udev, eudev, and mdev in that it offers *per-process access control* in a portable manner.

Project Goals
-------------
* **Portable Architecture**.  Unlike devfs or udev, vdev is designed to be portable across (modern) *nix.  As such, all OS-specific functionality is abstracted out of the core logic, and the core logic makes minimal assumptions about the underlying OS's device API.  In addition, the architecture supports multiple hardware event sources, including non-OS sources (such as another vdev instance, or modifications to an existing /dev tree).
* **Event-driven**.  Like devfs and udev, vdev leverages the underlying OS's hardware event-notification APIs.  Device nodes appear and disappear as the device itself becomes available or unavailable.
* **Advanced Access Control**.  Unlike existing systems, vdev filters access to device nodes based on which process is asking, in addition to filtering on user ID, group ID, and mode.  This obviates the need for using setuid/setgid to allow otherwise-unprivileged binaries to access privileged device nodes.  It also obviates the need for a session manager (such as systemd-logind) to mediate access to device nodes.
* **Container-Friendly**.  vdev runs without modification within containers and jails.  Like devfs, it exposes only the devices the root context wishes the contained process to see.

Project Non-Goals
-----------------
* **Init system integration**.  vdev is meant to run with full functionality regardless of which init system, daemon manager, service manager, plumbing layer, etc. the user runs, since they address orthogonal concerns.  It will coexist with and complement them, but neither forcibly replace them nor require them to be present.

Dependencies
------------
* libc
* libstdc++
* libpthread
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

To install, type:

    $ make install

The binaries will be installed by default to /usr/local/bin.
