#!/bin/dash

# video4linux helper 

. "$VDEV_HELPERS/subr.sh"

# entry point 
# return 0 on success
# return 10 if the action is unknown
main() {

   local BUS INDEX VENDOR MODEL TYPE ID PATH TYPE_PLUS_INDEX TYPE INDEX VDEV_PERSISTENT_PATH RC VDEV_PROPERTIES V4L_DATA

   # removing? just blow the links away 
   if [ "$VDEV_ACTION" = "remove" ]; then 

      vdev_cleanup "$VDEV_METADATA"
      return 0
   fi

   # must be adding...
   if [ "$VDEV_ACTION" != "add" ]; then 

      vdev_error "Unknown action \'$VDEV_ACTION\'"
      return 10
   fi

   BUS=
   INDEX=
   VENDOR=
   MODEL=
   TYPE=

   ID=
   PATH=

   # get the type and index
   TYPE_PLUS_INDEX="$(echo "$VDEV_PATH" | /bin/sed 's/.*\/\([^/]\+\)/\1/g')"

   TYPE="$(echo $TYPE_PLUS_INDEX | /bin/sed 's/[0-9]\+$//g')"
   INDEX=

   if [ -f "$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/index" ]; then 
      INDEX=$(/bin/cat "$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/index")
   else 

      # no index? no persistent paths
      return 0
   fi

   # is this a USB device?
   if [ -n "$(echo "$VDEV_OS_DEVPATH" | /bin/grep 'usb')" ]; then 

      V4L_DATA="$($VDEV_HELPERS/stat_usb "$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH")"
      RC=$?

      if [ $RC -ne 0 ]; then 
         vdev_error "$VDEV_HELPERS/stat_usb $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH exit code $RC"
         return 20
      fi

      # import 
      eval "$V4L_DATA"
      
      test -n "$VDEV_USB_VENDOR" || exit 1
      test -n "$VDEV_USB_MODEL" || exit 1

      VENDOR=$VDEV_USB_VENDOR
      MODEL=$VDEV_USB_MODEL
      BUS="usb"
   fi

   # did we get everything to make an ID?
   if [ -n "$BUS" -a -n "$INDEX" -a -n "$VENDOR" -a -n "$MODEL" -a -n "$TYPE" ]; then
      
      ID="$BUS-${VENDOR}_${MODEL}-$TYPE-index${INDEX}"
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/v4l/by-id/$ID" "$VDEV_METADATA"
   fi

   # by-path 
   VDEV_PERSISTENT_PATH=
   V4L_DATA="$($VDEV_HELPERS/stat_path "$VDEV_MOUNTPOINT/$VDEV_PATH")"
   RC=$?

   if [ $RC -ne 0 ]; then 
      vdev_error "$VDEV_HELPERS/stat_path $VDEV_MOUNTPOINT/$VDEV_PATH exit status $RC"
      return 1
   fi

   # import 
   eval "$V4L_DATA"

   # did we get a path?
   [ -z "$VDEV_PERSISTENT_PATH" ] && exit 0

   if [ -n "$(echo "$VDEV_PATH" | /bin/egrep "video|vbi")" ]; then 
      
      # video v4l device 
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/v4l/by-path/$VDEV_PERSISTENT_PATH-video-index${INDEX}" "$VDEV_METADATA"

   elif [ -n "$(echo "$VDEV_PATH" | /bin/egrep "audio")" ]; then 

      # audio v4l device 
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/v4l/by-path/$VDEV_PERSISTENT_PATH-audio-index${INDEX}" "$VDEV_METADATA"
   fi

   # set up permissions...
   vdev_permissions $VDEV_VAR_V4L_OWNER:$VDEV_VAR_V4L_GROUP $VDEV_VAR_V4L_MODE "$VDEV_MOUNTPOINT/$VDEV_PATH"

   return 0
}


if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
