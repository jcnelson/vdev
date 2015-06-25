#!/bin/dash

# helper script to tag "power switch" devices
. "$VDEV_HELPERS/subr.sh"

if [ "$VDEV_ACTION" != "add" ]; then 
   # not going to tag device unless we're adding it
   exit 0
fi

if [ -z "$VDEV_MAJOR" ] || [ -z "$VDEV_MINOR" ]; then 
   # can't make a device ID 
   exit 0
fi

SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"

if vdev_subsystems "$SYSFS_PATH" | /bin/egrep "acpi"; then 
   
   vdev_add_tag "power-switch" "$VDEV_METADATA" "$VDEV_GLOBAL_METADATA"

elif vdev_drivers "$SYSFS_PATH" | /bin/egrep "thinkpad_acpi"; then 
   
   vdev_add_tag "power-switch" "$VDEV_METADATA" "$VDEV_GLOBAL_METADATA"
fi

exit 0
