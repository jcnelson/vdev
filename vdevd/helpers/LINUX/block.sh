#!/bin/sh

# helper to link a block device to /dev/block/$MAJOR:$MINOR

/bin/mkdir -p $VDEV_MOUNTPOINT/block
/bin/ln -s ../$VDEV_PATH $VDEV_MOUNTPOINT/block/$VDEV_MAJOR:$VDEV_MINOR

