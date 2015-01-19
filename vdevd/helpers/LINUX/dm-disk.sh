#!/bin/sh

# helper for generating dm block device paths.
DM_QUERY="/sbin/dmsetup -j $VDEV_MAJOR -m $VDEV_MINOR --noudevrules --noudevsync --noheadings --columns info"

# get name and UUID, for /dev/disk/by-id
# TODO: WWN
DM_NAME=$($DM_QUERY -oname)
DM_UUID=$($DM_QUERY -oUUID)

if [ -n "$DM_NAME" ]; then
   
   /bin/mkdir -p $VDEV_MOUNTPOINT/disk/by-id/
   /bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/dm-name-$DM_NAME


   # add link to /dev/mapper as well
   /bin/mkdir -p $VDEV_MOUNTPOINT/mapper
   /bin/ln -s ../$VDEV_PATH $VDEV_MOUNTPOINT/mapper/$DM_NAME
fi

if [ -n "$DM_UUID" ]; then

   /bin/mkdir -p $VDEV_MOUNTPOINT/disk/by-id
   /bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/dm-uuid-$DM_UUID
fi

