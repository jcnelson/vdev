#!/bin/sh

# helper to link a block device to /dev/block/$MAJOR:$MINOR

. $VDEV_HELPERS/subr.sh

case "$VDEV_ACTION" in 

   add)

      vdev_symlink ../$VDEV_PATH $VDEV_MOUNTPOINT/block/$VDEV_MAJOR:$VDEV_MINOR $VDEV_METADATA
      ;;

   remove)

      vdev_rmlinks $VDEV_METADATA
      ;;

   *)
      vdev_fail 1 "Unknown action '$VDEV_ACTION'"
      ;;
esac

exit 0

