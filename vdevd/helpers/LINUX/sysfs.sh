#!/bin/dash

# helper to store device metadata from sysfs to the OS metadata database.
# in particular:
# * map a device's sysfs path to its metadata directory

. "$VDEV_HELPERS/subr.sh"

# entry point 
# return 0 on success
# return 1 on failure to symlink
main() {

   local SYSFS_PATH SERIALIZED_PATH

   if [ "$VDEV_ACTION" = "remove" ]; then 
      return 0
   fi
   
   if [ "$VDEV_PATH" = "UNKNOWN" ]; then 
      
      # unknown device path.  generate one.
      VDEV_METADATA="$VDEV_GLOBAL_METADATA/dev"/"$(vdev_device_id)"
      /bin/mkdir -p "$VDEV_METADATA"
   fi
   
   if [ "$VDEV_ACTION" != "add" ]; then 
      vdev_rmlinks "$VDEV_METADATA"
   fi
   
   SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"
   SERIALIZED_PATH="$(vdev_serialize_path "$SYSFS_PATH")"

   # remember where this device's metadata directory is
   # put it in the *global* metadata directory; as in, /dev/metadata/sysfs/$SERIALIZED_PATH --> $VDEV_METADATA
   vdev_symlink "$VDEV_METADATA" "$VDEV_GLOBAL_METADATA/sysfs/$SERIALIZED_PATH" "$VDEV_METADATA"
   return $?
}


if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi