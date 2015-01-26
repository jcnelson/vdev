#!/bin/sh

# link a disk device into /dev/disk/by-id, using its ATA ID

DISK_ID=$($VDEV_HELPERS/stat_ata $VDEV_MOUNTPOINT/$VDEV_PATH)
STAT_ATA_RET=$?

# verify ata_id succeeded; otherwise we're done (can't label by id)
test 0 -ne $STAT_ATA_RET && exit 0

# verify that we got a disk ID; otherwise we're done (no label to set)
test -z "$DISK_ID" && exit 0

# put label into place
DISK_NAME="ata-$DISK_ID"

# see if this is a partition...
if [ $VDEV_OS_DEVTYPE == "partition" ]; then 

   PART=$(/bin/cat /sys/dev/block/$VDEV_MAJOR\:$VDEV_MINOR/partition)

   test -z $PART && exit 0

   # include partition ID in the disk name
   DISK_NAME=$DISK_NAME-part${PART}
fi

# install by-id
/bin/mkdir -p $VDEV_MOUNTPOINT/disk/by-id
/bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-id/$DISK_NAME

# get UUID
UUID=$(/sbin/blkid | /bin/grep $VDEV_PATH | /bin/sed 's/[^ ]* UUID="\([^ ]*\)" .*/\1/g')

test -z "$UUID" && exit 0

# install by-uuid
/bin/mkdir -p $VDEV_MOUNTPOINT/disk/by-uuid
/bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/disk/by-uuid/$UUID

exit 0
