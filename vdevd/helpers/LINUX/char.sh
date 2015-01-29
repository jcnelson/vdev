#!/bin/sh

# helper to link a char device to /dev/char/$MAJOR:$MINOR

case "$VDEV_ACTION" in

   add)

      /bin/mkdir -p $VDEV_MOUNTPOINT/char
      /bin/ln -s ../$VDEV_PATH $VDEV_MOUNTPOINT/char/$VDEV_MAJOR:$VDEV_MINOR
      ;;

   remove)

      /bin/rm -f $VDEV_MOUNTPOINT/char/$VDEV_MJOR:$VDEV_MINOR
      ;;

   test)

      echo "ln -s ../$VDEV_PATH $VDEV_MOUNTPOINT/char/$VDEV_MAJOR:$VDEV_MINOR"
      ;;

esac

exit 0
