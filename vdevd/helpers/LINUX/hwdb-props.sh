#!/bin/dash

# Helper to insert a device's hardware database files 

. "$VDEV_HELPERS/subr.sh"
. "$VDEV_HELPERS/subr-hwdb.sh"

# entry point 
main() {

   local OLDIFS _PROP_NAME_AND_VALUE _PROP_NAME _PROP_VALUE
   
   if [ "$VDEV_ACTION" = "remove" ]; then 
      return 0
   fi

   if [ "$VDEV_PATH" = "UNKNOWN" ]; then 
      
      # unknown device path.  generate a metadata directory at least.
      VDEV_METADATA="$VDEV_GLOBAL_METADATA/dev"/"$(vdev_device_id)"
      /bin/mkdir -p "$VDEV_METADATA"
   fi

   if ! hwdb_available; then 
      return 0
   fi

   # skip loop devices, unless they're in use 
   if [ -z "$(/sbin/losetup -ln | /bin/grep "$VDEV_PATH")" ]; then 
      return 0
   fi

   # insert extra properties, if we have a hwdb and sufficient device information
   hwdb_enumerate "$VDEV_OS_MODALIAS" "$VDEV_OS_DEVPATH" "$VDEV_OS_SUBSYSTEM" "$VDEV_METADATA" "$VDEV_OS_SYSFS_MOUNTPOINT" | \
   while read -r _PROP_NAME_AND_VALUE; do 
      
      OLDIFS="$IFS"
      IFS="="
      
      set -- "$_PROP_NAME_AND_VALUE"

      IFS="$OLDIFS"

      if [ $# -ne 2 ]; then 
         # broken line
         continue
      fi
      
      _PROP_NAME="$1"
      _PROP_VALUE="$2"
      
      IFS="$OLDIFS"

      if [ -n "$_PROP_NAME" ] && [ -n "$_PROP_VALUE" ]; then 

         # put the property
         vdev_add_property "$_PROP_NAME" "$_PROP_VALUE" "$VDEV_METADATA"
      fi
      
   done

   return 0
}


if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi