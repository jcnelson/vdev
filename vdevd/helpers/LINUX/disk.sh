#!/bin/dash

# vdev helper for setting up symlinks to disks and partitions 
# works for ATA, SATA, and USB disks

. "$VDEV_HELPERS/subr.sh"

# if we're removing this disk, just blow away its symlinks
if [ "$VDEV_ACTION" = "remove" ]; then 

   vdev_rmlinks "$VDEV_METADATA"
   exit 0
fi

# otherwise, we're adding links.  Make sure...
if [ "$VDEV_ACTION" != "add" ]; then 

   vdev_fail 10 "Unknown action '$VDEV_ACTION'"
fi

# is this an ATA disk?
DISK_TYPE=$(test -n "$(echo "$VDEV_OS_DEVPATH" | /bin/grep -i ata)" && echo "ata")

if [ -z "$DISK_TYPE" ]; then
   
   # is this a USB disk, then?
   DISK_TYPE=$(test -n "$(echo "$VDEV_OS_DEVPATH" | /bin/grep -i usb)" && echo "usb")
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
      eval $($VDEV_HELPERS/stat_ata "$VDEV_MOUNTPOINT/$VDEV_PATH")
      STAT_RET=$?

      # disk id is the serial number
      DISK_ID=$VDEV_ATA_SERIAL

      # disk wwn is given by stat_ata 
      DISK_WWN=$VDEV_ATA_WWN
      
      ;;

   usb)
      
      # USB disk
      eval $($VDEV_HELPERS/stat_usb "$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH")
      STAT_RET=$?

            
      # disk id is the serial number
      DISK_ID=$VDEV_USB_SERIAL
      ;;

   *)
      
      # unknown type
      vdev_fail 2 "Unknown disk type '$DISK_TYPE'"
      ;;

esac

# verify stat'ing succeeded; otherwise we're done (can't label by id)
test 0 -ne $STAT_RET && vdev_fail 3 "stat_${DISK_TYPE} exited with $STAT_RET"

# verify that we got a disk ID; otherwise we're done (no label to set)
test -z "$DISK_ID" && vdev_fail 4 "unknown disk ID"

# put label into place
DISK_NAME="$DISK_TYPE-$DISK_ID"
PART_NAME=

# see if this is a partition...
if [ "$VDEV_OS_DEVTYPE" = "partition" ]; then 

   PART=$(/bin/cat "$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/partition")

   test -z $PART && vdev_fail 5 "unknown partition"

   # include partition ID in the disk name
   PART_NAME="part${PART}"
   DISK_NAME="$DISK_NAME-$PART_NAME"
fi

# get disk UUID, LABEL, PARTUUID
UUID=
LABEL=
PARTUUID=
PARTLABEL=

if [ -x /sbin/blkid ]; then 
   eval $(/sbin/blkid -o export "$VDEV_MOUNTPOINT/$VDEV_PATH")
else
   vdev_warn "Could not find blkid in /sbin/blkid.  $VDEV_MOUNTPOINT/disk/by-*/ symlinks will not be added."
fi

# get disk WWN, if set 
WWN=
if [ -n "$DISK_WWN" ]; then
   WWN="wwn-$DISK_WWN"

   if [ -n "$PART_NAME" ]; then
      WWN="$WWN-$PART_NAME"
   fi
fi

  
# add the disk
vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/$DISK_NAME" "$VDEV_METADATA"

if [ -n "$UUID" ]; then
   vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-uuid/$UUID" "$VDEV_METADATA"
fi

if [ -n "$LABEL" ]; then 
   vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-label/$LABEL" "$VDEV_METADATA"
fi

if [ -n "$WWN" ]; then
   vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/$WWN" "$VDEV_METADATA"
fi

if [ -n "$PARTUUID" ]; then 
   vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-partuuid/$PARTUUID" "$VDEV_METADATA"
fi

if [ -n "$PARTLABEL" ]; then 
   vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-partlabel/$PARTLABEL" "$VDEV_METADATA"
fi

# is this a physical volume?
PVS_RC=
LVM2_PV_UUID=

if [ -x /sbin/pvs ]; then 
   PVS="/sbin/pvs --nameprefixes --noheadings $VDEV_MOUNTPOINT/$VDEV_PATH"
   eval $($PVS -o pv_uuid)
   PVS_RC=$?
else
   vdev_warn "Could not find pvs in /sbin/pvs.  LVM physical volume symlinks in $VDEV_MOUNTPOINT/disk/by-id will not be created."
fi

if [ $PVS_RC -eq 0 -a -n "$LVM2_PV_UUID" ]; then 
   
   # this is a PV
   vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/lvm-pv-uuid-$LVM2_PV_UUID" "$VDEV_METADATA"

fi

# set ownership and bits 
vdev_permissions root:disk 0660 "$VDEV_MOUNTPOINT/$VDEV_PATH"

exit 0
