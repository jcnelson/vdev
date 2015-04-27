#!/bin/sh

# vdev helper for setting up symlinks to disks and partitions 
# works for ATA, SATA, and USB disks

. $VDEV_HELPERS/subr.sh

# if we're removing this disk, just blow away its symlinks
if [ "$VDEV_ACTION" = "remove" ]; then 

   remove_links $VDEV_METADATA
   exit 0
fi

# otherwise, we're adding links.  Make sure...
if [ "$VDEV_ACTION" != "add" ]; then 

   fail 10 "Unknown action '$VDEV_ACTION'"
fi

# is this an ATA disk?
DISK_TYPE=$(test -n "$(echo $VDEV_OS_DEVPATH | /bin/grep -i ata)" && echo "ata")

if [ -z "$DISK_TYPE" ]; then
   
   # is this a USB disk, then?
   DISK_TYPE=$(test -n "$(echo $VDEV_OS_DEVPATH | /bin/grep -i usb)" && echo "usb")
fi

# is this a disk type this script supports?
test -z "$DISK_TYPE" && exit 0

STAT_RET=
DISK_ID=
DISK_WWN=
STAT_RET=0

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
      fail 2 "Unknown disk type '$DISK_TYPE'"
      ;;

esac

# verify stat'ing succeeded; otherwise we're done (can't label by id)
test 0 -ne $STAT_RET && fail 3 "stat_${DISK_TYPE} exited with $STAT_RET"

# verify that we got a disk ID; otherwise we're done (no label to set)
test -z "$DISK_ID" && fail 4 "unknown disk ID"

# put label into place
DISK_NAME="$DISK_TYPE-$DISK_ID"
PART_NAME=

# see if this is a partition...
if [ "$VDEV_OS_DEVTYPE" = "partition" ]; then 

   PART=$(/bin/cat $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/partition)

   test -z $PART && fail 5 "unknown partition"

   # include partition ID in the disk name
   PART_NAME="part${PART}"
   DISK_NAME="$DISK_NAME-$PART_NAME"
fi

# get disk UUID and LABEL
UUID=
LABEL=
eval $(/sbin/blkid -o export $VDEV_MOUNTPOINT/$VDEV_PATH)

# get disk WWN, if set 
WWN=
if [ -n "$DISK_WWN" ]; then
   WWN="wwn-$DISK_WWN"

   if [ -n "$PART_NAME" ]; then
      WWN="$WWN-$PART_NAME"
   fi
fi

  
# add the disk
if [ -n "$UUID" ]; then
   add_link ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-uuid/$UUID $VDEV_METADATA 
fi

add_link ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/$DISK_NAME $VDEV_METADATA

if [ -n "$LABEL" ]; then 
   add_link ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-label/$LABEL $VDEV_METADATA
fi

if [ -n "$WWN" ]; then
   add_link ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/$WWN $VDEV_METADATA
fi

# is this a physical volume?
PVS="/sbin/pvs --nameprefixes --noheadings $VDEV_MOUNTPOINT/$VDEV_PATH"
eval $($PVS -o pv_uuid)
PVS_RC=$?

if [ $PVS_RC -eq 0 -a -n "$LVM2_PV_UUID" ]; then 
   
   # this is a PV
   add_link ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/lvm-pv-uuid-$LVM2_PV_UUID $VDEV_METADATA

fi

# set ownership and bits 
/bin/chown root.disk $VDEV_MOUNTPOINT/$VDEV_PATH
/bin/chmod 0660 $VDEV_MOUNTPOINT/$VDEV_PATH

exit 0
