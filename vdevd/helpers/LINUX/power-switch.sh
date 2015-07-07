#!/bin/dash

# helper script to tag "power switch" devices
. "$VDEV_HELPERS/subr.sh"

# entry point 
# return 0 on success
main() {

   local SYSFS_PATH

   if [ "$VDEV_ACTION" != "add" ]; then 
      # not going to tag device unless we're adding it
      return 0
   fi

   if [ -z "$VDEV_MAJOR" ] || [ -z "$VDEV_MINOR" ]; then 
      # can't make a device ID 
      return 0
   fi

   SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"

   if [ -n "$(vdev_subsystems "$SYSFS_PATH" | /bin/egrep "acpi")" ]; then 
      
      vdev_add_tag "power-switch" "$VDEV_METADATA" "$VDEV_GLOBAL_METADATA"

   elif [ -n "$(vdev_drivers "$SYSFS_PATH" | /bin/egrep "thinkpad_acpi")" ]; then 
      
      vdev_add_tag "power-switch" "$VDEV_METADATA" "$VDEV_GLOBAL_METADATA"
   fi

   return 0
}


if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi