#!/bin/dash

# helper to link a char device to /dev/char/$MAJOR:$MINOR

. "$VDEV_HELPERS/subr.sh"

# main method 
# return 0 on success
# return 1 on unknown action
main() {
   case "$VDEV_ACTION" in

      add)

         vdev_symlink "../$VDEV_PATH" "$VDEV_MOUNTPOINT/char/$VDEV_MAJOR:$VDEV_MINOR" "$VDEV_METADATA"
         return $?
         ;;

      remove)

         vdev_cleanup "$VDEV_METADATA"
         return $?
         ;;

      *)
         vdev_error "Unknown action '$VDEV_ACTION'"
         return 1
         ;;
   esac

   return 0
}

if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
