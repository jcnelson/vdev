How to Install vdev
===================

This document is meant to help both packagers and individuals wishing to install vdev from source do so as painlessly as possible.  However, the reader is expected to have basic command-line proficiency, as well as familiarity with their bootloader of choice.

The steps in this document are lightly tested.  Please report bugs to the Devuan mailing list, or to `judecn@devuan.org`.

Step 1: Compile and Install vdev
--------------------------------

The steps to check out, compile and install vdev and host-specific configuration files are as follows:

    $ git clone https://github.com/jcnelson/vdev
    $ cd vdev
    $ make -C libvdev
    $ sudo make -C libvdev install
    $ make -C vdevd
    $ sudo make -C vdevd install
    $ make -C example
    $ sudo make -C example install

As a result of these commands, you will have installed `vdevd` to `/sbin/vdevd`, vdev's helpers to `/lib/vdev/`, vdev's host-specific configuration to `/etc/vdev/`, vdev's System V init script to `/etc/init.d/`, and vdev's support library to `/lib/libvdev.so`.

NB: The host-specific parts of the configuration includes `/etc/vdev/ifnames.conf`, which encodes the persistent network interface names.

Step 2: Generate an Initramfs
-----------------------------

To boot with vdev in an initramfs, you must additionally generate an initramfs image that contains it.  This can only be done after vdev has been installed.  To do so, run:

    $ make initramfs

The initramfs image file will be generated under `example/initrd.img-$KERNEL_VERSION`, where `$KERNEL_VERSION` is the build host's running kernel version (i.e. from `uname -r`).

Step 3: Install the Initramfs
-----------------------------

Once the initramfs image has been generated, you must configure your bootloader to use it.  The steps to do so are bootloader-specific.  For GRUB2, this can be achieved simply by copying the image file into `/boot` and running `sudo update-grub2`.

**WARNING:** Be sure to back up your old initramfs image first!
**WARNING:** Be sure you know how to boot from the backed-up initramfs image if this one does not work!
