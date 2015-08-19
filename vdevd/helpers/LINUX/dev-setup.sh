#!/bin/dash

# THIS SCRIPT IS NOT MEANT TO BE RUN MANUALLY.
# 
# Pre-seed initialization script to vdevd.
# Populate /dev with extra files that vdevd won't create, but are 
# necessary for correct operation.
# Write to stdout any "$type $name $major $minor $PARAM=$VALUE..." strings for
# devices that vdevd should process.
#   $type is "c" or "b" for char or block devices, respectively 
#   $name is the relative path to the device 
#   $major is the major device number (pass 0 if not needed)
#   $minor is the minor device number (pass 0 if not needed)
#   $PARAM=$VALUE is an OS-specific parameter to be passed as a
#      VDEV_OS_${PARAM} environment variable to vdev helpers
#
# It is okay to create device nodes here, but be sure to output the above information
# for it as well (so vdevd knows to run actions for it, instead of stumble over it).
# 
# Requires /proc and /sys to be mounted

VDEV_MOUNTPOINT="$1"
VDEV_CONFIG_FILE="$2"

# mount the squashfs hardware database.
# generate the loopback device file needed to do so--create it, and inform vdevd to set it up.
# $1    hardware database file
# $2    the name of the loopback device to create
# TODO: exponential back-off, in case we race
attach_hwdb() {

   local _HWDB_PATH _HWDB_LOOP _LOOP_MINOR
   
   _HWDB_PATH="$1"
   _HWDB_LOOP="$2"

   # deduce minor number 
   _LOOP_MINOR="${_HWDB_LOOP##loop}"
   
   # attempt to mount the hardware database
   /bin/mknod "$VDEV_MOUNTPOINT/$_HWDB_LOOP" b 7 $_LOOP_MINOR
   RC=$?
   
   if [ $RC -ne 0 ]; then 
      return $RC
   fi 
   
   /sbin/losetup "$VDEV_MOUNTPOINT/$_HWDB_LOOP" "$_HWDB_PATH"
   RC=$?
   
   if [ $RC -ne 0 ]; then 
      
      /bin/rm -f "$VDEV_MOUNTPOINT/$_HWDB_LOOP"
      return $RC
   fi 
   
   /bin/mount -t squashfs "$VDEV_MOUNTPOINT/$_HWDB_LOOP" "$VDEV_MOUNTPOINT/metadata/hwdb"
   RC=$?
   
   if [ $RC -ne 0 ]; then 
      
      /sbin/losetup -d "$VDEV_MOUNTPOINT/$_HWDB_LOOP"
      /bin/rm -f "$VDEV_MOUNTPOINT/$_HWDB_LOOP"
      return $RC
   fi 

   # make sure vdevd still processes the loop device
   echo "b $_HWDB_LOOP 7 $_LOOP_MINOR"

   return 0
}


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
    
    [ -e "$VDEV_MOUNTPOINT/$name" ] && continue

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
    major=$(echo $arg | /bin/sed 's/^\([^:]\+\):.*/\1/g')
    minor=$(echo $arg | /bin/sed 's/^[^:]\+:\(.*\)/\1/g')

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

      devpath=$(/bin/readlink "$sys_link" | /bin/sed 's/\.\.\///g')
      devpath="/$devpath"

      subsystem=$(/bin/readlink "/sys/$devpath/subsystem" | /bin/sed "s/[^/]*\///g")
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

      /sbin/restorecon "$VDEV_MOUNTPOINT/$name"
    fi
  done
}


umask 022

# add /dev/fd
/bin/ln -sf /proc/self/fd "$VDEV_MOUNTPOINT/fd"

# add /dev/core 
/bin/ln -sf /proc/kcore "$VDEV_MOUNTPOINT/core"

# feed /dev/null into vdev, and make it locally
echo "c null 1 3"
/bin/mknod "$VDEV_MOUNTPOINT/null" c 1 3

# add /dev/stdin, /dev/stdout, /dev/stderr
/bin/ln -sf /proc/self/fd/0 "$VDEV_MOUNTPOINT/stdin"
/bin/ln -sf /proc/self/fd/1 "$VDEV_MOUNTPOINT/stdout"
/bin/ln -sf /proc/self/fd/2 "$VDEV_MOUNTPOINT/stderr"

# add MAKEDEV
if [ -e /sbin/MAKEDEV ]; then
   /bin/ln -sf /sbin/MAKEDEV "$VDEV_MOUNTPOINT/MAKEDEV"
else
   /bin/ln -sf /bin/true "$VDEV_MOUNTPOINT/MAKEDEV"
fi

# add /dev/shm
test -d "$VDEV_MOUNTPOINT/shm" || /bin/ln -sf /run/shm "$VDEV_MOUNTPOINT/shm"

# add events 
for dirp in "events" "events/global"; do
   test -d "$VDEV_MOUNTPOINT/$dirp" || /bin/mkdir -p "$VDEV_MOUNTPOINT/$dirp"
done

# add vdevd's metadata directories 
for dirp in "metadata/dev" "metadata/hwdb"; do
   test -d "$VDEV_MOUNTPOINT/$dirp" || /bin/mkdir -p "$VDEV_MOUNTPOINT/$dirp"
done

# add udev compatibility, including udev-formatted events
for dirp in "metadata/udev/tags" "metadata/udev/data" "metadata/udev/links" "metadata/udev/events/global"; do
   test -d "$VDEV_MOUNTPOINT/$dirp" || /bin/mkdir -p "$VDEV_MOUNTPOINT/$dirp"
done

# attach the hardware database, if we have one 
HWDB_VAR="$(/bin/fgrep "hwdb=" "$VDEV_CONFIG_FILE")"
LOOP_VAR="$(/bin/fgrep "hwdb_loop=" "$VDEV_CONFIG_FILE" | /bin/egrep "=loop[0-9]+$")"

VDEV_HWDB_PATH=
LOOP_NAME=

if [ -n "$HWDB_VAR" ]; then 
   eval "$HWDB_VAR"
   VDEV_HWDB_PATH="$hwdb"
fi

if [ -n "$LOOP_VAR" ]; then 
   eval "$LOOP_VAR"
   LOOP_NAME="$hwdb_loop"
fi

if [ -z "$LOOP_NAME" ]; then 
   # pick the next free one 
   # TODO: exponential back-off, in case we race
   LOOP_NAME="$(/sbin/losetup -f | /bin/sed "s/[^/]*\///g")"
fi 

if [ -n "$VDEV_HWDB_PATH" ] && [ -x /sbin/losetup ]; then 
   
   attach_hwdb "$VDEV_HWDB_PATH" "$LOOP_NAME"
   RC=$?
   if [ $RC -ne 0 ]; then 
      echo "WARN: Failed to mount hardware database" > /proc/self/fd/2
   fi
fi

feed_static_nodes_kmod

exit 0

