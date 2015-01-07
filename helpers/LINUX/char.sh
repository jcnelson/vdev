#!/bin/sh

# helper to link a char device to /dev/char/$MAJOR:$MINOR

/bin/mkdir -p $VDEV_MOUNTPOINT/char
/bin/ln -s ../$VDEV_PATH $VDEV_MOUNTPOINT/char/$VDEV_MAJOR:$VDEV_MINOR

