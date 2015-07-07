#!/bin/dash

. "$VDEV_HELPERS/subr.sh"


# load firmware dir 
FIRMWARE_VAR="$(/bin/fgrep "firmware=" "$VDEV_CONFIG_FILE")"
VDEV_FIRMWARE_DIR=

if [ -n "$FIRMWARE_VAR" ]; then 
   eval "$FIRMWARE_VAR"
   VDEV_FIRMWARE_DIR="$firmware"
fi

if [ -z "$VDEV_FIRMWARE_DIR" ] || ! [ -d "$VDEV_FIRMWARE_DIR" ]; then 

   vdev_error "Firmware directory not found at $VDEV_FIRMWARE_DIR"
   return 1
fi

# load firmware for a device 
# $1   the sysfs device path 
# $2   the path to the firmware 
# return 0 on success
# return 1 on error
firmware_load() {
   
   local _SYSFS_PATH _FIRMWARE_PATH _SYSFS_FIRMWARE_PATH _RC
   
   _SYSFS_PATH="$1"
   _FIRMWARE_PATH="$2"
   _SYSFS_FIRMWARE_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$_SYSFS_PATH"

   test -e "$_SYSFS_FIRMWARE_PATH/loading" || return 1
   test -e "$_SYSFS_FIRMWARE_PATH/data" || return 1
   
   echo 1 > "$_SYSFS_FIRMWARE_PATH/loading"
   /bin/cat "$_FIRMWARE_PATH" > "$_SYSFS_FIRMWARE_PATH/data"
   
   _RC=$?
   if [ $_RC -ne 0 ]; then 
      # abort 
      echo -1 > "$_SYSFS_FIRMWARE_PATH/loading"
   else 
      # success
      echo 0 > "$_SYSFS_FIRMWARE_PATH/loading"
   fi

   return $_RC
}


# entry point
# return 0 on success 
# return 1 if we couldn't load the firmware
main() {

   local FIRMWARE_VAR VDEV_FIRMARE_DIR _RC 
   
   # if instructed to load firmware, do so
   if [ -n "$VDEV_OS_FIRMWARE" -a -n "$VDEV_OS_DEVPATH" ]; then 
      
      firmware_load "$VDEV_OS_DEVPATH" "$VDEV_FIRMWARE_DIR/$VDEV_OS_FIRMWARE"
      
      _RC=$?

      if [ $_RC -ne 0 ]; then 
         vdev_error "Failed to load formware $VDEV_FIRMWARE_DIR/$VDEV_OS_FIRMWARE for $VDEV_OS_DEVPATH"
         return 1
      fi
      
   fi

   return 0
}
 
  
if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi