How to Install vdev
===================

This document is meant to help both packagers and individuals wishing to install vdev from source do so as painlessly as possible.  However, the reader is expected to have basic command-line proficiency, familiarity with their bootloader of choice, and familiarity with installing and removing system services.

The steps in this document are lightly tested, and are **specific to Debian and Devuan**.  Please report bugs to the Devuan mailing list, or to `judecn@devuan.org`.

Step 1: Compile and Install vdev
--------------------------------


The steps to check out, compile and install vdev, libudev-compat, and host-specific configuration files to your root partition are as follows.  These steps will disable udevd and libudev, and make it so vdevd and its libraries are used in place of it.

**WARNING:** These steps will disable udev.  If vdev does not work correctly for you, you are expected to know how to restore udev.

**BEFORE RUNNING THESE COMMANDS:**  You will need to back up your copies of `libudev.so.*` from udev.  They are often found in the **multiarch /lib** (such as `/lib/x86_64-linux-gnu/`).  You should move them out of your linker's search path, so libudev-linked software will use libudev-compat's `libudev.so.*` files (moving them to a directory on your root partition, like `/lib/libudev.bak/`, should be fine).  The instructions below install libudev-compat's `libudev.so.*` files to `/lib` to avoid accidentally overwriting udev's `libudev.so.*` in the multiarch `/lib` directory.

    $ git clone https://github.com/jcnelson/vdev
    $ cd vdev
    $ make -C vdevd PREFIX= 
    $ sudo make -C vdevd install PREFIX=
    $ make -C libudev-compat PREFIX= INCLUDE_PREFIX=/usr
    $ sudo make -C libudev-compat install PREFIX= INCLUDE_PREFIX=/usr
    $ sudo update-rc.d udev disable
    $ sudo update-rc.d udev-finish disable
    $ make -C example
    $ sudo make -C example install

As a result of these commands, you will have installed `vdevd` to `/sbin/vdevd`, vdev's helpers to `/lib/vdev/`, vdev's host-specific configuration to `/etc/vdev/`, vdev's System V init script to `/etc/init.d/`, and libudev-compat to `/lib/libudev.so.1.5.2`.

**AFTER RUNNING THESE COMMANDS:** You will need to restart any software that was using `libudev.so.1*`.  This includes the X server in most cases.  Use `lsof(8)` to find a full listing.  You can ignore any software that uses `libudev.so.0*` (such as Chrome and Chromium).

Step 2: Generate an Initramfs
-----------------------------

To boot with vdev in an initramfs, you must additionally generate an initramfs image that contains it.  This can only be done after vdev has been installed to your root partition.

    $ make -C example initramfs

The initramfs image file will be generated under `example/initrd.img-$KERNEL_VERSION`, where `$KERNEL_VERSION` is the build host's running kernel version (i.e. from `uname -r`).


Step 3: Install the Initramfs
-----------------------------

**BEFORE YOU DO ANYTHING:**  You should ensure that you will be able to recover if this initramfs does not boot for you.  This usually involves ensuring that your bootloader always has a valid entry to a vanilla initramfs image.  With GRUB2, you can test the vdev initramfs without installing it.  Simply copy it to `/initrd.img.vdev`, reboot, edit your boot script, and replace the path to the given initrd with `/initrd.img.vdev`.

Once the initramfs image has been generated and you're comfortable with it, you must configure your bootloader to use it.  The steps to do so are bootloader-specific.  For GRUB2, this can be achieved simply by copying the image file into `/boot` and running `sudo update-grub2`.

**WARNING:** Be sure to back up your old initramfs image first!

**WARNING:** Be sure you know how to boot from the backed-up initramfs image if this one does not work!
