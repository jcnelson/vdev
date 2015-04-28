#!/bin/sh


# helper for generating dm block device paths and symlinks.

. $VDEV_HELPERS/subr.sh

# if removing, just blow away the links
if [ "$VDEV_ACTION" = "remove" ]; then

   vdev_rmlinks $VDEV_METADATA
   exit 0
fi

# make sure we're adding... 
if [ "$VDEV_ACTION" != "add" ]; then 

   vdev_fail 10 "Unknown action '$VDEV_ACTION'"
fi

DM_QUERY="/sbin/dmsetup -j $VDEV_MAJOR -m $VDEV_MINOR --noudevrules --noudevsync --noheadings --columns info"

# get name and UUID, for /dev/disk/by-id
DM_NAME=$($DM_QUERY -oname)
DM_UUID=$($DM_QUERY -oUUID)

if [ -n "$DM_NAME" ]; then

   vdev_symlink ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/dm-name-$DM_NAME $VDEV_METADATA
   vdev_symlink ../$VDEV_PATH $VDEV_MOUNTPOINT/mapper/$DM_NAME $VDEV_METADATA
fi

if [ -n "$DM_UUID" ]; then

   vdev_symlink ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/dm-uuid-$DM_UUID $VDEV_METADATA
fi

# also add by-uuid link 
UUID=
eval $(/sbin/blkid -o export $VDEV_MOUNTPOINT/$VDEV_PATH)

if [ -n "$UUID" ]; then
   vdev_symlink ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-uuid/$UUID $VDEV_METADATA 
fi

# create all logical volume links
LVS="/sbin/lvs -o all --noheadings --nameprefixes"
$LVS 2>/dev/null | \
while read lvs_vars; do
   
   LVM2_LV_KERNEL_MAJOR=
   LVM2_LV_KERNEL_MINOR=
   LVM2_VG_NAME=
   LVM2_VG_FULL_NAME=
   LVM2_VG_FMT=
   
   eval $lvs_vars

   # find this mapped device's LVM info...   
   if [ $LVM2_LV_KERNEL_MAJOR -ne $VDEV_MAJOR ]; then 
      continue
   fi

   if [ $LVM2_LV_KERNEL_MINOR -ne $VDEV_MINOR ]; then 
      continue
   fi

   # only lvm2-formatted supported at this time...
   if [ "$LVM2_VG_FMT" != "lvm2" ]; then 
      vdev_warn "Only lvm2-formatted volumes are supported"
      continue
   fi

   # create the LVM link for this mapped device
   /bin/mkdir -p $VDEV_MOUNTPOINT/$LVM2_VG_NAME
   vdev_symlink ../$VDEV_PATH $VDEV_MOUNTPOINT/$LVM2_VG_NAME/$LVM2_LV_NAME $VDEV_METADATA

done

# set ownership and bits 
vdev_permissions root.disk 0660 $VDEV_MOUNTPOINT/$VDEV_PATH

exit 0
