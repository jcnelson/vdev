#!/bin/dash

# vdev helper to set up symlinks to optical (CD, DVD) devices, based on capability

. "$VDEV_HELPERS/subr.sh"

# removing? blow away the symlinks 
if [ "$VDEV_ACTION" = "remove" ]; then 

   vdev_rmlinks $VDEV_METADATA
   exit 0
fi

# otherwise, make sure we're adding 
if [ "$VDEV_ACTION" != "add" ]; then 

   vdev_fail 10 "Unknown action '$VDEV_ACTION'"
fi

# set up capability environment variables 
eval $($VDEV_HELPERS/stat_optical "$VDEV_MOUNTPOINT/$VDEV_PATH")
STAT_RC=$?

# verify that we stat'ed the optical device...
test 0 -ne $STAT_RC && vdev_fail 1 "Not an optical device"

add_capability_link() {

   # conditionally set the symlink if the capability is given 
   
   OPTICAL_DEV=$1
   CAPABILITY=$2
   TARGET=$3

   if [ $CAPABILITY = "1" ]; then

      vdev_symlink "$VDEV_PATH" "$VDEV_MOUNTPOINT/$TARGET" "$VDEV_METADATA"
   fi
}


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

exit 0
