#!/bin/dash

# Subroutines for interfacing with vdev's hardware database.

VDEV_HWDB_PATH="$VDEV_MOUNTPOINT/metadata/hwdb"
if ! [ -d "$VDEV_HWDB_PATH" ]; then 

   # no hwdb
   VDEV_HWDB_PATH=
fi

# get the hwdb prefix from an input class 
# $1    the input class 
hwdb_input_class_to_prefix() {

   local _INPUT_CLASS _HWDB_PREFIX

   _INPUT_CLASS="$1"
   _HWDB_PREFIX=""

   case $_INPUT_CLASS in 
      
      kbd)
         
         _HWDB_PREFIX="keyboard"
         ;;

      mouse|joystick)

         _HWDB_PREFIX="mouse"
         ;;

      *)
         
         _HWDB_PREFIX="$_INPUT_CLASS"
         ;;
   esac

   echo "$_HWDB_PREFIX"
   return 0
}


# do we have enough information to query the hwdb?
# check the caller's environment for the appropriate variables.
# no arguments 
# return 0 if so
# return 1 if not 
# see also: hwdb_enumerate (how it checks)
hwdb_available() {

   if [ -z "$VDEV_HWDB_PATH" ]; then 
      return 1
   fi 
   
   if [ -n "$VDEV_OS_MODALIAS" ]; then 
      return 0
   fi

   if [ -n "$VDEV_OS_DEVPATH" ]; then 

      if [ "$VDEV_OS_SUBSYSTEM" = "usb" ] || [ "$VDEV_OS_SUBSYSTEM" = "usb_device" ]; then 
         
         return 0
      else 
         
         return 1
      fi
   fi

   return 1
}


# use the hwdb to look up extra properties for this device 
# $1    modalias string (defaults to $VDEV_OS_MODALIAS)
# $2    devpath (defualts to $VDEV_OS_DEVPATH)
# $3    subsystem (defualts to $VDEV_OS_SUBSYSTEM)
# $4    device metadata (defaults to $VDEV_METADATA)
# return 0 on success, and print the properties to stdout
hwdb_enumerate() {

   local _MODALIAS _SUBSYSTEM _DEVPATH _HWDB_ARGS _INPUT_CLASS _PREFIX _METADATA _SYSFS_MOUNTPOINT

   _MODALIAS="$1"
   _DEVPATH="$2"
   _SUBSYSTEM="$3"
   _METADATA="$4"
   _SYSFS_MOUNTPOINT="$5"
   _INPUT_CLASS=""

   if [ -z "$VDEV_HWDB_PATH" ]; then 
      # no hwdb
      return 0
   fi
   
   if [ -z "$_MODALIAS" ]; then 
      _MODALIAS="$VDEV_OS_MODALIAS"
   fi

   if [ -z "$_SUBSYSTEM" ]; then 
      _SUBSYSTEM="$VDEV_OS_SUBSYSTEM"
   fi

   if [ -z "$_DEVPATH" ]; then 
      _DEVPATH="$VDEV_OS_DEVPATH"
   fi

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if [ -z "$_SYSFS_MOUNTPOINT" ]; then 
      _SYSFS_MOUNTPOINT="/sys/"
   fi

   _HWDB_ARGS="-h $VDEV_HWDB_PATH -s $_SYSFS_MOUNTPOINT"
   
   if [ -n "$_MODALIAS" ]; then 
      
      if [ "$_SUBSYSTEM" = "input" ]; then 
         
         if [ -f "$_METADATA/properties" ]; then 
            
            # use the input class as the prefix 
            _INPUT_CLASS="$(/bin/fgrep "VDEV_INPUT_CLASS=" "$_METADATA/properties")"
            _INPUT_CLASS="${_INPUT_CLASS%%VDEV_INPUT_CLASS=}"

            if [ -n "$_INPUT_CLASS" ]; then 
               _PREFIX="$(hwdb_input_class_to_prefix "$_INPUT_CLASS")"

               _HWDB_ARGS="$_HWDB_ARGS -P $_PREFIX"
            fi
         fi
      fi

      _HWDB_ARGS="$_HWDB_ARGS $_MODALIAS"
   
   elif [ -n "$_DEVPATH" ]; then 
      
      if [ "$_SUBSYSTEM" = "usb" ] || [ "$_SUBSYSTEM" = "usb_device" ]; then 
         
         _HWDB_ARGS="$_HWDB_ARGS -D $_DEVPATH -S $_SUBSYSTEM"
      
      else

         # not enough info to search the hwdb
         return 0
      fi
   
   else
   
      # not enough info to search the hwdb
      return 0
   fi
   
   if [ -n "$_HWDB_ARGS" ]; then (

      # print to stdout
      "$VDEV_HELPERS/hwdb.sh" $_HWDB_ARGS
   
   ) fi

   return 0
}
