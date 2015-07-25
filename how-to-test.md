How to Test vdev
================

This document is meant to help early adopters help me test vdev, and make sure it works with a wide variety of hardware.  The reader is expected to be familiar with basic shell commands and have basic familiarity with Makefiles.

Step 1: Compile and Install vdev
--------------------------------

The steps to compile and prepare vdevd for testing are as follows:

    $ git clone https://github.com/jcnelson/vdev vdev
    $ cd vdev
    $ make -C vdevd

As a result of these commands, you will have checked out the latest copy of vdev's source code, and built and installed the hotplug daemon `vdevd`.

Step 2: Set up a Fake /dev
--------------------------

`vdevd` is capable of running on a live system with `udevd` or `mdev` or the like, but we will need to ensure it creates device nodes somewhere else besides `/dev`.  To do so, we'll set up a `tmpfs` mount:

    $ mkdir vdev-test
    $ sudo mount -t tmpfs none vdev-test/

`vdevd` will populate the directory `vdev-test/` with device nodes in the next step.

Step 3: Generate a Fake /dev
----------------------------

`vdevd` has the option of scanning the contents of `/sys` and exiting once it finishes.  We call this "run once" mode.  It's useful for generating a static `/dev`, or updating a previously-generated static `/dev` with only the device files that correspond to hardware plugged into your computer.  We will use this feature to test `vdevd`.

From the root of the source code repository you checked out in step 1, the command to have `vdevd` generate a fake `/dev` is:

    $ sudo vdevd/vdevd -v2 -c example/vdevd.conf -l /tmp/vdev.log --once vdev-test/

`vdevd` should print out some runtime setup information to your console, run for a few seconds, and exit normally.  If you run `ls vdev-test/`, you should see a bunch of device files.  All debugging information will have been written to `/tmp/vdev.log`.

Step 4: Inspect the Fake /dev and Logfile
-----------------------------------------

If you'd like, feel free to explore the `vdev-test/` directory and skim through the `/tmp/vdev.log` logfile, and note any missing or extra device files and symlinks.

You might notice the directory `vdev-test/vdev/`.  This directory contains device attributes for each device processed (specifically, the contents of the `uevent` packet `vdevd` processed, as well as a few other things documented in the Appendix).  Programs that need to monitor the system for devices and query device properties need only look in this directory.

Step 5: Send Me a Listing of /dev and the Logfile
-------------------------------------------------

Once you're satisfied, please run `tree /dev` and `tree vdev-test/`, and send me the resulting directory tree listings so I can figure out what device files and symlinks `vdevd` failed to create.  Also, please send me the logfile `/tmp/vdev.log`.

If either the listings or the `/tmp/vdev.log` file contains information you'd rather not share, please take the time to trim that information out (and please note it in the listings).  In particular `/tmp/vdev.log` will contain a full listing of all the hardware plugged into your computer, including device serial numbers and network MAC addresses.  I will keep the information to myself and not share with anyone, but nevertheless the information will be sent over the public Internet.

Once you're satisfied, please email them to `judecn@devuan.org`.

Thank you very much for reading!

Appendix A: A Crash-Course to vdevd
-----------------------------------

`vdevd` has a `--help` switch that prints out the directives it understands.  In particular, you can:

* make it run in the foreground with `-f`.  This is great if you want to watch it process hardware events (but this requires `-v1` or `-v2`).
* change the log verbosity with `-v $LEVEL`.  Substitute `$LEVEL` with 0 for logging only drastic errors; 1 for logging non-critical notices, and 2 for logging debug messages.
* change the log file with `-l $PATH_TO_LOGFILE`.  If you omit this, `vdevd` logs to stdout.
* write a PID file with `-p $PATH_TO_PIDFILE`.  This is useful only when running `vdevd` as a daemon.

`vdevd` works internally by bufferring up device events from the kernel, matching device events against "actions," and running an action's associated script if it matches.  The vdev project comes with a set of actions that are meant to make `vdevd` behave as close as possible to udev.  You can find them in `example/actions/`, and you can find the Linux-specific scripts and binaries the actions execute in `vdevd/helpers/LINUX/`.

Actions are defined as ini-files.  Unless stated otherwise, all action fields are optional.  All fields must match the device event for the action's command to be executed.  The fields are:

* `event`: **Required**.  Use "add" to match a device-add event, "remove" to match a device-remove event, "change" to match a device-change event (such as a battery level changing), or "any" to match any device event.
* `path`: This is an extended POSIX regular expression that matches the kernel-given device path (such as `sda` or `input/mice`) to the action.
* `type`: This is either "block" or "char", which will match the event only if it it is for a block or character device, respectively.  Not all devices are representable by block or character device files, however, which is why this field is optional.
* `OS_*`: Any field prefixed with `OS_` matches an OS-specific device attribute.  On Linux, these include names of `uevent` fields.  For example, an action can match a device's subsystem using the `OS_SUBSYSTEM` field.  If an empty value is given (e.g. "`OS_SUBSYSTEM=`"), then the action will match any value as long as the device attribute is present in the device event (for example, a Linux `uevent` packet that does not have `SUBSYSTEM` defined will not match an action with `OS_SUBSYSTEM=`).

Once a device event is encountered that matches all of an action's fields, `vdevd` runs the commands specified by the action's ini file.  The fields that describe the commands are:

* `rename_command`:  This is a shell command to run to generate the path to the device file to create.  It will be evaluated after the device event matches the action, but before the device file is created.  The command must write the desired path to stdout.  Its output will be truncated at 4,096 characters (which is `PATH_MAX` on most filesystems).  Note that not all devices have paths given by the kernel.
* `command`:  This is the shell command to run once the device file has been successfully created.  Unless specified otherwise, `vdevd` will run this as a subprocess--it will wait until the command has completed before processing the next action.
* `async`:  If set to "True", this tells `vdevd` to continue processing the action immediately after running its `command`.  This is useful for long-running device setup tasks, where it is undesirable to block `vdevd`.
* `daemonlet`:  If set to "True", this tells `vdevd` to run the command as a *daemonlet*.  See "Advanced Device Handling" below for a description of what this means.

**Caveat**:  `vdevd` matches a device event against actions according to their files' lexographic order.  Moreover, it processes device events sequentially, in the order in which they arrive.  This is also true for the Linux port--the kernel's SEQNUM field is ignored at this time (but is passed on to actions).

`vdevd` communicates device information to action commands using environment variables.  When a `command` or `rename_command` runs, the following environment variables will be set:

* `$VDEV_ACTION`:  This is the `event` field of the action (i.e. "add", "remove", "change", or "any").
* `$VDEV_DAEMONLET`: If set to 1, then the command is being invoked as a daemonlet (see the "Advanced Device Handling" subsection below).
* `$VDEV_CONFIG_FILE`: This is the absolute path to the configuration file `vdevd` is using.
* `$VDEV_HELPERS`:  This is the absolute path to the directory containing `vdevd`'s helper programs.
* `$VDEV_INSTANCE`:  This is a randomly-generated string that corresponds to this running intance of `vdevd`.  It will be different on each invocation of `vdevd`.
* `$VDEV_GLOBAL_METADATA`:  The absolute path to the metadata directory into which `vdevd` writes metadata (e.g. `/dev/metadata`).
* `$VDEV_LOGFILE`:  If specified, this is the path to `vdevd`'s logfile.
* `$VDEV_MAJOR`:  If the device event corresponds to a block or character device, this is the device file's major number.
* `$VDEV_METADATA`:  If the device is given a path (either by the kernel or the `rename_command` output), this is the absolute path to the directory to contain the device's metadata.
* `$VDEV_MINOR`:  If the device event corresponds to a block or character device, this is the device file's minor number.
* `$VDEV_MODE`:  If the device event corresponds to a block or character device, this is the string "block" or "char" (respectively).
* `$VDEV_MOUNTPOINT`:  This is the absolute path to the directory in which the device files will be created.  For example, this is usually `/dev`.
* `$VDEV_OS_*`:  Any OS-specific device attributes will be encoded by name, prefixed with `VDEV_OS_`.  For example, on Linux, the SUBSYSTEM field in the device's `uevent` packet will be exported as `$VDEV_OS_SUBSYSTEM`.  Similarly, the DEVPATH field in a device's `uevent` packet will be exported as `$VDEV_OS_DEVPATH`.
* `$VDEV_PATH`:  If the device is given a path (either by the kernel or the `rename_command` output), this is the path to the device file *relative* to `$VDEV_MOUNTPOINT`.  The absolute path to the device file can be found by `$VDEV_MOUNTPOINT/$VDEV_PATH`.  Note that not all devices have device files (such as batteries, PCI buses, etc.).


**Advanced Device Handling**

By default, `vdevd` will `fork()` and `exec()` the `command` for each device request in the system shell.  However, as an optimization, `vdevd` can run the command as a *daemonlet*.  This means that the `command` will be expected to stay resident once it proceses a device request, and receive and process subsequent device requests from `vdevd` instead of simply exiting after each one.  Doing so saves `vdevd` from having to `fork()` and `exec()` the same command over and over again for common types of devices, and lets it avoid having to repeatly incur long `command` start-up times.  It also allows the administrator to define stateful or long-running actions that can work on sets of devices.

The programming model for daemonlet commands is as follows:
1. `vdevd` will `fork()` and `exec()` the command directly.  It will *not* invoke the system shell to run it.
2. `vdevd` will write a sequence of newline-terminated strings to the command's `stdin`, followed by an empty newline string.  These strings encode the request's environment variables, and are in the form `NAME=VALUE\n'.
3. The `command` is expected to write an ASCII-encoded exit code to `stdout` to indicate the success/failure of processing the request.  0 indicates success, and non-zero indicates failure.

If the `command` crashes or misbehaves, `vdevd` will log as such and attempt to restart it.


Appendix B: Booting with vdevd
-------------------------------

**NOTE: These instructions are Debian- and Devuan-specific, and very hacky.  Use at your own risk.**
**WARNING: Readers are expected to know how to fix a broken initramfs and a broken bootsystem if they try this.**

Running `make && sudo make install` will get you most of the way towards installing vdev.  But to use it, you will need to disable udev, enable vdev, and rebuild your initramfs to include vdev instead of udev.

On Debian and most Debian-derived distributions, it is possible to generate an initramfs image with this command:

    $ cd example/ && make initramfs

This will generate an initramfs image in `example/`, which can be installed with your bootloader of choice.

To enable vdev and disable udev in the init system, the command is 

    $ cd example/ && make initscript-install

I'm still working on the packaging scripts that will do all of this automatically.


Misc
----

Thanks for reading!  Please send any questions, comments, and bug reports to the Devuan mailing list (`dng@lists.dyne.org`)
