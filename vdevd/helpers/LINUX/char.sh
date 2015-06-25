#!/bin/dash

# helper to link a char device to /dev/char/$MAJOR:$MINOR

. "$VDEV_HELPERS/subr.sh"

case "$VDEV_ACTION" in

   add)

      vdev_symlink "../$VDEV_PATH" "$VDEV_MOUNTPOINT/char/$VDEV_MAJOR:$VDEV_MINOR" "$VDEV_METADATA"
      ;;

   remove)

      vdev_cleanup "$VDEV_METADATA"
      ;;

   *)
      vdev_fail 1 "Unknown action '$VDEV_ACTION'"
      ;;
esac

exit 0
