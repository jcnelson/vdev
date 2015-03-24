#!/bin/sh

# Pre-vdev initialization script.
# usage: dev-setup.sh PATH_TO_DEV
# Populate /dev with extra files that vdev won't create, but are 
# necessary for correct operation.

VDEV_MOUNTPOINT=$1

# add /dev/fd
/bin/ln -sf /proc/self/fd $VDEV_MOUNTPOINT/fd

# add /dev/core 
/bin/ln -sf /proc/kcore $VDEV_MOUNTPOINT/core

# add /dev/null
/bin/mknod /dev/null c 1 3
/bin/chmod 0666 /dev/null

# add /dev/stdin, /dev/stdout, /dev/stderr
/bin/ln -sf /proc/self/fd/0 $VDEV_MOUNTPOINT/stdin
/bin/ln -sf /proc/self/fd/1 $VDEV_MOUNTPOINT/stdout
/bin/ln -sf /proc/self/fd/2 $VDEV_MOUNTPOINT/stderr

# add MAKEDEV
if [ -e /sbin/MAKEDEV ]; then
   /bin/ln -sf /sbin/MAKEDEV $VDEV_MOUNTPOINT/MAKEDEV
else
   /bin/ln -sf /bin/true $VDEV_MOUNTPOINT/MAKEDEV
fi

# add /dev/shm
test -d $VDEV_MOUNTPOINT/shm || /bin/ln -sf /run/shm $VDEV_MOUNTPOINT/shm

# add static device files
# use kmod to create all device files for modules for the currently-running kernel version.
# see kmod(8)
make_static_nodes_kmod() {

  [ -e /lib/modules/$(uname -r)/modules.devname ] || return 0
  [ -x /bin/kmod ] || return 0

  /bin/kmod static-nodes --format=tmpfiles --output=/proc/self/fd/1 | \
  while read type name mode uid gid age arg; do

    [ -e $name ] && continue
   
    case "$type" in

      c|b) mknod -m $mode $name $type $(echo $arg | sed 's/:/ /') ;;

      d) mkdir $name ;;

      *) echo "unparseable line ($type $name $mode $uid $gid $age $arg)" >&2 ;;

    esac

    if [ -x /sbin/restorecon ]; then

      /sbin/restorecon $name
    fi
  done
}

make_static_nodes_kmod

exit 0

