#!/bin/sh

# Pre-seed initialization script to vdevd.
# usage: dev-setup.sh /path/to/dev
# Populate /dev with extra files that vdevd won't create, but are 
# necessary for correct operation.
# Write to stdout any "$type $name $PARAM=$VALUE..." strings for
# devices that vdevd should process before creating.

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


# guess the subsystem if /sys/dev/{block,char}/$major:$minor isn't helpful.
# print it to stdout
# $1  type of device (c for char, b for block)
# $2  name of device relative to /dev
guess_subsystem() {

   local devtype=$1
   local devname=$2

   if [ "$devname" = "snd/seq" ]; then 
      echo "sound"
   fi
}


# feed static device nodes into vdevd.
# use kmod to query all device files for modules for the currently-running kernel version.
# see kmod(8)
feed_static_nodes_kmod() {

  [ -e /lib/modules/$(uname -r)/modules.devname ] || return 0
  [ -x /bin/kmod ] || return 0

  /bin/kmod static-nodes --format=tmpfiles --output=/proc/self/fd/1 | \
  while read type name mode uid gid age arg; do

    # strip /dev/ from $name
    name=${name#/dev/}
   
    # strip bang from $type 
    type=${type%%\!}
     
    [ -e $VDEV_MOUNTPOINT/$name ] && continue

    # skip directories--vdevd can make them 
    if [ "$type" = "d" ]; then 
       continue
    fi

    # only process character and block devices 
    if [ "$type" != "c" -a "$type" != "b" ]; then 
       echo "Invalid device type $type" >/proc/self/fd/2
       continue
    fi

    # look up major/minor 
    major=$(echo $arg | /bin/sed -r 's/^([^:]+):.*/\1/g')
    minor=$(echo $arg | /bin/sed -r 's/^[^:]+:(.*)/\1/g')

    # look up devpath and subsystem
    sys_link=
    devpath=
    params=
    case "$type" in
      "c")

         sys_link="/sys/dev/char/$major:$minor"
         ;;

      "b")
         
         sys_link="/sys/dev/block/$major:$minor"
         ;;
    esac

    # is there a sysfs entry for this device?  there might not be...
    if ! [ -L $sys_link ]; then 

      subsystem=$(guess_subsystem $type $name)
    
    else

      devpath=$(/bin/readlink $sys_link | /bin/sed -r 's/\.\.\///g')
      devpath="/$devpath"

      subsystem=$(/bin/readlink /sys/$devpath/subsystem | /bin/sed -r "s/[^/]*\///g")
    fi

    # build up params list 
    if [ -n "$devpath" ]; then 

       params="$params DEVPATH=$devpath"
    fi 

    if [ -n "$subsystem" ]; then 

       params="$params SUBSYSTEM=$subsystem"
    fi

    # feed into vdevd via stdout
    echo "$type $name $major $minor $params"
    
    # keep SELinux happy
    if [ -x /sbin/restorecon ]; then

      /sbin/restorecon $VDEV_MOUNTPOINT/$name
    fi
  done
}

feed_static_nodes_kmod

exit 0

