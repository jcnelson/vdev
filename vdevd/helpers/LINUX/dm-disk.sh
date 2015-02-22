#!/bin/sh


# helper for generating dm block device paths.

. $VDEV_HELPERS/subr.sh

# if removing, just blow away the links
if [ "$VDEV_ACTION" == "remove" ]; then

   remove_links $VDEV_METADATA
   exit 0
fi

# make sure we're adding... 
if [ "$VDEV_ACTION" != "add" ]; then 

   fail 10 "Unknown action '$VDEV_ACTION'"
fi

DM_QUERY="/sbin/dmsetup -j $VDEV_MAJOR -m $VDEV_MINOR --noudevrules --noudevsync --noheadings --columns info"

# get name and UUID, for /dev/disk/by-id
DM_NAME=$($DM_QUERY -oname)
DM_UUID=$($DM_QUERY -oUUID)

if [ -n "$DM_NAME" ]; then

   add_link ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/dm-name-$DM_NAME $VDEV_METADATA
   add_link ../$VDEV_PATH $VDEV_MOUNTPOINT/mapper/$DM_NAME $VDEV_METADATA
fi

if [ -n "$DM_UUID" ]; then

   add_link ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/dm-uuid-$DM_UUID $VDEV_METADATA
fi

exit 0
