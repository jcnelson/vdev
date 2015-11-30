#!/bin/dash

# helper to accumulate properties for a network device 

. "$VDEV_HELPERS/subr.sh"

# entry point for daemonlet 
# return 0 on success
# return 2 if a helper fails
main() {

   local ALL_PROPS SYSFS_PATH RC VDEV_PROPERTIES ALL_PROPS NET_DATA VDEV_PERSISTENT_PATH

   if [ "$VDEV_ACTION" != "add" ]; then 
      return 0
   fi

   ALL_PROPS=

   # full sysfs path
   SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"

   # net name and MAC 
   VDEV_PROPERTIES=
   NET_DATA="$($VDEV_HELPERS/stat_net "$VDEV_OS_INTERFACE")"
   RC=$?

   if [ $RC -ne 0 ]; then 
      vdev_error "$VDEV_HELPERS/stat_net \"$VDEV_OS_INTERFACE\" rc = $RC"
      return 2
   fi

   # import 
   eval "$NET_DATA"

   if [ -n "$VDEV_PROPERTIES" ]; then 
      ALL_PROPS="$ALL_PROPS $VDEV_PROPERTIES"
   fi

   # persistent path 
   VDEV_PROPERTIES=
   VDEV_PERSISTENT_PATH=
   NET_DATA="$($VDEV_HELPERS/stat_path "$SYSFS_PATH")"
   RC=$?

   if [ $RC -ne 0 ]; then 
      vdev_error "$VDEV_HELPERS/stat_path $SYSFS_PATH rc = $RC"
      return 2
   fi

   # import 
   eval "$NET_DATA"

   if [ -n "$VDEV_PROPERTIES" ]; then 
      ALL_PROPS="$ALL_PROPS $VDEV_PROPERTIES"
   fi

   if [ "$VDEV_PATH" = "UNKNOWN" ]; then 
      # use device-id metadata directory
      VDEV_METADATA="$VDEV_GLOBAL_METADATA/dev"/"$(vdev_device_id)"
      /bin/mkdir -p "$VDEV_METADATA"
      VDEV_PATH=""
   fi

   # save all properties
   vdev_add_properties "$VDEV_METADATA" $VDEV_PROPERTIES

   return 0
}


if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
