#!/bin/sh

# vdev helper for disks and partitions 
# works for ATA, SATA, and USB disks.

DISK_TYPE=

# is this an ATA disk?
DISK_TYPE=$(test -n "$(echo $VDEV_OS_DEVPATH | /bin/grep -i ata)" && echo "ata")

if [ -z "$DISK_TYPE" ]; then
   
   # is this a USB disk, then?
   DISK_TYPE=$(test -n "$(echo $VDEV_OS_DEVPATH | /bin/grep -i usb)" && echo "usb")
fi

# do we have a disk type?
test -z "$DISK_TYPE" && exit 1

STAT_RET=
DISK_ID=
DISK_WWN=

case "$DISK_TYPE" in

   ata)

      # (S)ATA disk
      eval $($VDEV_HELPERS/stat_ata $VDEV_MOUNTPOINT/$VDEV_PATH)
      STAT_RET=$?

      # disk id is the serial number
      DISK_ID=$VDEV_ATA_SERIAL

      # disk wwn is given by stat_ata 
      DISK_WWN=$VDEV_ATA_WWN
      
      ;;

   usb)
      
      # USB disk
      eval $($VDEV_HELPERS/stat_usb $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH)
      STAT_RET=$?

            
      # disk id is the serial number
      DISK_ID=$VDEV_USB_SERIAL
      ;;

   *)
      
      # unknown type
      echo "Unknown disk type" >&2
      exit 1
      ;;

esac

# verify stat'ing succeeded; otherwise we're done (can't label by id)
test 0 -ne $STAT_RET && exit 0

# verify that we got a disk ID; otherwise we're done (no label to set)
test -z "$DISK_ID" && exit 0

# put label into place
DISK_NAME="$DISK_TYPE-$DISK_ID"
PART_NAME=

# see if this is a partition...
if [ "$VDEV_OS_DEVTYPE" = "partition" ]; then 

   PART=$(/bin/cat $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/partition)

   test -z $PART && exit 0

   # include partition ID in the disk name
   PART_NAME="part${PART}"
   DISK_NAME="$DISK_NAME-$PART_NAME"
fi

# get disk UUID
UUID=$(/sbin/blkid | /bin/grep "$VDEV_PATH:" | /bin/sed 's/[^ ]* UUID="\([^ ]*\)" .*/\1/g')

# get disk WWN, if set 
WWN=
if [ -n "$DISK_WWN" ]; then
   WWN="wwn-$DISK_WWN"

   if [ -n "$PART_NAME" ]; then
      WWN="$WWN-$PART_NAME"
   fi
fi

# (un)install by-uuid
case "$VDEV_ACTION" in

   add)
   
      if [ -n "$UUID" ]; then 
         /bin/mkdir -p $VDEV_MOUNTPOINT/disk/by-uuid
         /bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-uuid/$UUID
      fi

      /bin/mkdir -p $VDEV_MOUNTPOINT/disk/by-id
      /bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/$DISK_NAME

      if [ -n "$WWN" ]; then
         /bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/$WWN
      fi

      ;;

   remove)

      if [ -n "$UUID" ]; then
         /bin/rm -f $VDEV_MOUNTPOINT/disk/by-uuid/$UUID
      fi
      
      /bin/rm -f $VDEV_MOUNTPOINT/disk/by-id/$DISK_NAME

      if [ -n "$WWN" ]; then 
         /bin/rm -f $VDEV_MOUNTPOINT/disk/by-id/$WWN 
      fi 
      
      ;;

   test)

      if [ -n "$UUID" ]; then 
         echo "ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-uuid/$UUID"
      fi

      echo "ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/$DISK_NAME"

      if [ -n "$WWN" ]; then 
         echo "ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/$WWN"
      fi
      
      ;;

   *)

      exit 2

esac

exit 0
