#!/bin/dash

# helper to accumulate properties for a network device 

. "$VDEV_HELPERS/subr.sh"

if [ "$VDEV_ACTION" != "add" ]; then 
   exit 0
fi

ALL_PROPS=

# full sysfs path
SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"

# net name and MAC 
VDEV_PROPERTIES=
eval "$($VDEV_HELPERS/stat_net "$VDEV_OS_INTERFACE")"
RC=$?

if [ $RC -ne 0 ]; then 
   vdev_fail 1 "$VDEV_HELPERS/stat_net \"$VDEV_OS_INTERFACE\" rc = $RC"
fi

if [ -n "$VDEV_PROPERTIES" ]; then 
   ALL_PROPS="$ALL_PROPS $VDEV_PROPERTIES"
fi

# persistent path 
VDEV_PROPERTIES=
eval "$($VDEV_HELPERS/stat_path "$SYSFS_PATH")"

if [ -n "$VDEV_PROPERTIES" ]; then 
   ALL_PROPS="$ALL_PROPS $VDEV_PROPERTIES"
fi

# save all properties
vdev_add_properties "$VDEV_METADATA" $VDEV_PROPERTIES

exit 0