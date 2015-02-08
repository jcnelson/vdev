#!/bin/sh

# vdev helper to set up symlinks to optical (CD, DVD) devices, based on capability

STAT_OPTICAL=$VDEV_HELPERS/stat_optical

# set up capability environment variables 
eval $($STAT_OPTICAL $VDEV_MOUNTPOINT/$VDEV_PATH)
STAT_OPTICAL_EXIT=$?

# verify that we stat'ed the optical device...
test 0 -ne $STAT_OPTICAL_EXIT && exit 0

test_device_symlink() {

   TARGET=$1

   echo "ln -s $VDEV_PATH $VDEV_MOUNTPOINT/$TARGET"
   return $?
}

set_device_symlink() {
   
   TARGET=$1

   /bin/ln -s $VDEV_PATH $VDEV_MOUNTPOINT/$TARGET
   return $?
}

remove_device_symlink() {
   
   TARGET=$1

   /bin/rm -f $VDEV_MOUNTPOINT/$TARGET
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

case "$VDEV_ACTION" in

   add)
   
      set_device_symlink "cdrom"
      
      add_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_R "cdrw"
      add_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_RW "cdrw"
      add_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD "dvd"
      add_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_R "dvdrw"
      add_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_RAM "dvdrw"
      
      ;;

   remove)

      remove_device_symlink "cdrom"
      
      remove_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_R "cdrw"
      remove_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_RW "cdrw"
      remove_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD "dvd"
      remove_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_R "dvdrw"
      remove_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_RAM "dvdrw"

      ;;

   test)

      
      test_device_symlink "cdrom"
      
      test_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_R "cdrw"
      test_device_symlink $VDEV_PATH $VDEV_OPTICAL_CD_RW "cdrw"
      test_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD "dvd"
      test_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_R "dvdrw"
      test_device_symlink $VDEV_PATH $VDEV_OPTICAL_DVD_RAM "dvdrw"

      ;;

esac

exit 0
