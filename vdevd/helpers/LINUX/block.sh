#!/bin/sh

# helper to link a block device to /dev/block/$MAJOR:$MINOR

case "$VDEV_ACTION" in 

   add)

      /bin/mkdir -p $VDEV_MOUNTPOINT/block
      /bin/ln -s ../$VDEV_PATH $VDEV_MOUNTPOINT/block/$VDEV_MAJOR:$VDEV_MINOR
      ;;

   remove)

      /bin/rm -f $VDEV_MOUNTPOINT/block/$VDEV_MAJOR:$VDEV_MINOR
      ;;

   test)

      echo "ln -s ../$VDEV_PATH $VDEV_MOUNTPOINT/block/$VDEV_MAJOR:$VDEV_MINOR"
      ;;
esac

exit 0

