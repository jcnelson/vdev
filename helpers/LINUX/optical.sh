#!/bin/sh

# vdev helper to set up symlinks to optical (CD, DVD) devices, based on capability

STAT_OPTICAL=$VDEV_HELPERS/stat_optical

# set up capability environment variables 
eval $($STAT_OPTICAL $VDEV_MOUNTPOINT/$VDEV_PATH)

set_device_symlink() {
   
   TARGET=$1

   /bin/ln -s $VDEV_PATH $VDEV_MOUNTPOINT/$TARGET 2>/dev/null
   return $?
}

add_device_symlink() {

   # conditionally set the symlink if the capability is given 
   
   OPTICAL_DEV=$1
   CAPABILITY=$2
   TARGET=$3

   if [ $CAPABILITY == "1" ]; then
      set_device_symlink $TARGET
   fi

   return 0
}

# always add 'cdrom'
set_device_symlink "cdrom"

add_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_R "cdrw"
add_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_RW "cdrw"
add_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD "dvd"
add_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_R "dvdrw"
add_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_RAM "dvdrw"
