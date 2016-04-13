vdev: a device-file manager and filesystem
==========================================

**This system is a work-in-progress.  If you would like to help, please see the [Issue Tracker](https://github.com/jcnelson/vdev/issues)**.

Vdev is a portable userspace device-file manager for UNIX-like operating systems.  It differs from existing device-file managers in that it provides an optional filesystem interface that implements a *per-process view of /dev*, thereby giving the host administrator the means to control device node access based on arbitrary criteria (such as process session ID, process seat assignment, etc.).

The Linux port of vdev aims to be a drop-in replacement for udev.

More information is available in the [design document](http://judecnelson.blogspot.com/2015/01/introducing-vdev.html).

Project Goals
-------------
* **Portable Architecture**.  Vdev is designed to be portable across (modern) *nix.  It can interface with both the kernel and synthetic device event sources, including another vdev instance, or an existing `/dev` tree.  It can also be statically linked and can compile with multiple libc's.
* **Event-driven**.  Vdev's core logic is built around reacting to events from its back-end event source.  It creates and removes device files and metadata in response to devices being plugged in or unplugged.
* **Scriptable**.  Vdev aims to keep device management policy and mechanisms as separate as possible.  It offers an easy-to-understand programming model for matching a device event to a sequence of administrator-defined programs to handle it.
* **Advanced Access Control**.  Vdev comes with an optional userspace filesystem that lets the administrator control how individual processes see the files under `/dev`.  The criteria are programmatically selected and thus arbitrary--*any* process information can be used to control access.
* **Container Friendly**.  Vdev can run in containers and chroots, and offers the administrator an easy-to-understand way to restrict, filter, and modify the device events the contained vdevd will observe.  Importantly, the Linux port of vdev does *not* rely on netlink sockets to propagate events to client programs.
* **Sensible, Backwards-Compatible Defaults**.  Vdev comes with a reference configuration and set of helper programs that make it usable out-of-the-box, with no subsequent tweaking required.  The Linux port is a drop-in replacement for udev:  it generates an equivalent `/dev` tree, as well as an equivalent `/run/udev` tree.  It also ships with a "libudev-compat" library that is ABI-compatible with libudev 219.  Libudev-linked software such as the X.org X server can use vdev and libudev-compat without recompilation.  In addition, the Linux port interoperates with but does not depend on the `devtmpfs` filesystem.

Project Non-Goals
-----------------
* **Init system integration**.  Vdev is meant to run with full functionality regardless of which init system, daemon manager, service manager, plumbing layer, etc. the user runs, since they address orthogonal concerns.  It will coexist with and complement them, but neither forcibly replace them nor require them to be present.
* **New client libraries, language bindings, or Dbus APIs**.  Vdev will exprose all of the necessary state and device metadata via the /dev filesystem directly.  Any wrappers that present this information via higher-level APIs (such as libudev) will be supported only to provide backwards compatibility.

Dependencies
-----------

There are two binaries in vdev:  the hotplug daemon vdevd, and the userspace filesystem vdevfs.  You can use one without the other.  **If you only intend to replace udevd, you can ignore vdevfs.**

To build vdevd, you'll need:
* libc
* libpthread

For vdevfs, you'll need all of the above, plus:
* libfuse
* [libpstat](https://github.com/jcnelson/libpstat)
* [fskit](https://github.com/jcnelson/fskit)

**Recommended Packages**

Vdevd comes with a set of scripts that provide udev compatibility for the Linux port, and these scripts make use of the following packages.  Vdevd's scripts can work without them, but some functionality will be missing:
* lvm2
* iproute2

Vdev's libudev-compat library removes the netlink connection to udev in favor of creating and watching a process- and `udev_monitor`-specific directory, which vdevd's helper scripts discover and use to send serialized device events by writing them as files.  It is highly recommended that users install [eventfs](https://github.com/jcnelson/eventfs) and use it to host libudev-compat's event directories.  Eventfs is optimized for this use-case--it will ensure that libudev-compat's directories are backed by RAM, easily accessed in FIFO order, and automatically removed once the libudev-compat process that created them exits.


Building
--------

By default, everything installs under `/usr/local`.  To build and install everything with default options, run:

    $ make
    $ sudo make install

To build and install vdevd (with no configuration) to `/usr/local/sbin`, type:

    $ make -C vdevd OS=$OS_TYPE
    $ sudo make -C vdevd install

Substitute `$OS_TYPE` with:
* "LINUX" to build for Linux (the default value)
* "OPENBSD" to build for OpenBSD (coming soon)

`$OS_TYPE` defaults to "LINUX".

To build and install vdevd's default recommended configuration to `/usr/local/etc`, type:

    $ make -C example
    $ sudo make -C example install 

To build and install vdevd's hardware database to `/usr/local/lib`, type:

    $ make -C hwdb 
    $ sudo make -C hwdb install

To build and install libudev-compat to `/usr/local/lib/` and its headers to `/usr/local/include`, type:

    $ make -C libudev-compat 
    $ sudo make -C libudev-compat install

To build and install vdevfs to `/usr/local/sbin/`, type:

    $ make -C fs
    $ sudo make -C fs install

You can override the installation directories at build-time by setting the `PREFIX` variable on the command-line (e.g. `make -C vdevd PREFIX=/`), and you can specify an alternative installation root by setting `DESTDIR` at install-time (e.g. `sudo make -C vdevd install DESTDIR=/opt`).  You can also control where header files are installed by setting the `INCLUDE_PREFIX` variable (e.g. `make -C libudev-compat install PREFIX=/ INCLUDE_PREFIX=/usr`).

Replacing udev on Linux
-----------------------

If you want to replace udev with vdev, it is recommended that you install the binaries to your root partition and the libudev-compat headers to `/usr`.  This is done by setting `DESTDIR= PREFIX= INCLUDE_PREFIX=/usr` when installing.

If you have an initramfs and rely on udevd during early boot, you will need to rebuild your initramfs so it will start vdevd instead.  If you are installing on Debian or Devuan, please first read Appendix B of the [how-to-test.md](https://github.com/jcnelson/vdev/blob/master/how-to-test.md) document, since it contains instructions on how to install vdevd's init script and rebuild your initramfs via the Debian/Devuan initramfs tools.

**If you are replacing udev and libudev, you should back up their init scripts, header files, and libraries before installing vdev.  You may need to revert to them if vdev does not work for you.**

FAQ
---
* **Why another device manager?**  I want to control which processes have access to which device nodes based on criteria other than their user and group IDs.  For example, I want to filter access based on which login session the process belongs to, and I want to filter based on which container a process runs in.  Also, I want to have this functionality available on each of the *nix OSs I use on a (semi-)regular basis.  To the best of my knowledge, no device manager lets me do this (except perhaps Plan 9's per-process namespaces), so I wrote my own.
* **What is the license for vdev?**  Vdev code is available for use under the terms of either the [GPLv3+](https://github.com/jcnelson/vdev/blob/master/LICENSE.GPLv3%2B) or [ISC license](https://github.com/jcnelson/vdev/blob/master/LICENSE.ISC).  However, the Linux port of vdev ships with some optional Linux-specific [helper programs](https://github.com/jcnelson/vdev/tree/master/vdevd/helpers/LINUX) that were derived from udev.  They are available under the terms of the GPLv2 where noted in the source files.
* **Which Linux distributions are you targeting?**  Vdev is distro-agnostic, but I'm developing and testing primarily for [Debian](http://www.debian.org) and [Devuan](http://devuan.org).
* **Which BSDs/other UNIXes are you planning to support?**  OpenBSD for now.  Well-written, suitably-licensed pull requests that port vdev to these or other UNIX-like OSs will be gratefully accepted.
* **Does this project have anything to do with systemd and udev merging?**  It's not strictly related, but the pending merger definitely motivated me to start this project sooner rather than later.  I hope vdev's existence will help alleviate the tension between the pro- and anti-systemd crowds:
  * Linux users who don't want to use systemd can run vdev in place of udev.
  * Linux Users who prefer some other device manager besides udev or vdev but need to run software that depends on libudev can use libudev-compat (vdev's udev-compatibility helper programs are easily portable to other device managers).
  * Linux developers who need fine-grained device access controls but don't want to couple their software to systemd will have vdevfs as a portable, easy-to-use alternative.
  * Linux users do not need to choose between systemd and vdevfs, since they can coexist.
  * The systemd developers can tightly couple udev to systemd-PID-1 if they want, since non-systemd users will have an alternative.
  * **Result:** Everyone wins.
