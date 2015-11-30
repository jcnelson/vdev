#!/bin/dash

# vdev helper to set up symlinks to optical (CD, DVD) devices, based on capability

. "$VDEV_HELPERS/subr.sh"


# conditionally set the symlink if the capability is given 
# $1    the optical device path
# $2    the capability bit (0 or 1)
# $3    the symlink to create
add_capability_link() {

   OPTICAL_DEV="$1"
   CAPABILITY="$2"
   TARGET="$3"

   if [ $CAPABILITY = "1" ]; then

      vdev_symlink "$VDEV_PATH" "$VDEV_MOUNTPOINT/$TARGET" "$VDEV_METADATA"
   fi

   return 0
}


# entry point 
# return 0 on success
# return 10 on unknown action 
# return 2 if our optical helper fails 
main() {

   local OPTICAL_DATA
   
   # removing? blow away the symlinks 
   if [ "$VDEV_ACTION" = "remove" ]; then 

      vdev_cleanup "$VDEV_METADATA"
      exit 0
   fi

   # otherwise, make sure we're adding 
   if [ "$VDEV_ACTION" != "add" ]; then 

      vdev_error "Unknown action '$VDEV_ACTION'"
      return 10
   fi

   # set up capability environment variables 
   OPTICAL_DATA="$($VDEV_HELPERS/stat_optical "$VDEV_MOUNTPOINT/$VDEV_PATH")"
   STAT_RC=$?

   # verify that we stat'ed the optical device...
   if [ $STAT_RC -ne 0 ]; then 
      vdev_error "Not an optical device"
      return 2
   fi 

   # import 
   eval "$OPTICAL_DATA"

   # always add 'cdrom'
   vdev_symlink "$VDEV_PATH" "$VDEV_MOUNTPOINT/cdrom" "$VDEV_METADATA"

   # conditionally add symlinks, based on capability
   add_capability_link "$VDEV_PATH" $VDEV_OPTICAL_CD_R "cdrw"
   add_capability_link "$VDEV_PATH" $VDEV_OPTICAL_CD_RW "cdrw"
   add_capability_link "$VDEV_PATH" $VDEV_OPTICAL_DVD "dvd"
   add_capability_link "$VDEV_PATH" $VDEV_OPTICAL_DVD_R "dvdrw"
   add_capability_link "$VDEV_PATH" $VDEV_OPTICAL_DVD_RAM "dvdrw"

   # set ownership and bits 
   vdev_permissions root:cdrom 0660 "$VDEV_MOUNTPOINT/$VDEV_PATH"
}


if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
