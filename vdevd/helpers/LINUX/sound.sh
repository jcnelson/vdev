#!/bin/dash
# vim: set sts=4 expandtab:

# set up /dev/snd/by-path and /dev/snd/by-id 

. "$VDEV_HELPERS/subr.sh"

# entry point
# return 0 on success
# return 10 on unknown action
main() {

   local STAT_RC VDEV_PERSISTENT_PATH VDEV_PROPERTIES BUS SERIAL SOUND_DATA

   VDEV_PROPERTIES=""
   BUS=""
   SERIAL=""

   # if removing, just remove symlinks
   if [ "$VDEV_ACTION" = "remove" ]; then 

      vdev_cleanup "$VDEV_METADATA"
      return 0
   fi

   # make sure we're adding 
   if [ "$VDEV_ACTION" != "add" ]; then 

      vdev_error "Unknown action \"$VDEV_ACTION\""
      return 10
   fi

   # if we're dealing with ALSA controlC[0-9] files, set up persistent paths 
   if [ -n "$(echo $VDEV_PATH | /bin/egrep "controlC[0-9]+")" ]; then 

      SOUND_DATA="$($VDEV_HELPERS/stat_path "$VDEV_MOUNTPOINT/$VDEV_PATH")"
      STAT_RC=$?

      # verify that we got a persistent path 
      if [ $STAT_RC -eq 0 ]; then 

         # import 
         eval "$SOUND_DATA"

         if [ -n "$VDEV_PERSISTENT_PATH" ]; then
 
            # install the path 
            vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/snd/by-path/$VDEV_PERSISTENT_PATH" "$VDEV_METADATA"
         fi

      elif [ $STAT_RC -ne 0 ]; then 

         # failed to stat 
         vdev_error "$VDEV_HELPERS/stat_path \"$VDEV_MOUNTPOINT/$VDEV_PATH\" rc = $STAT_RC"
      fi
   fi

   SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"

   # if this is a USB device, then add by-id persistent path 
   if [ -n "$(vdev_subsystems "$SYSFS_PATH" | /bin/grep 'usb')" ]; then 

      SOUND_DATA="$($VDEV_HELPERS/stat_usb "$SYSFS_PATH")"
      STAT_RC=$?

      # did we get USB info?
      if [ $STAT_RC -ne 0 ]; then 

         # nope 
         vdev_error "$VDEV_HELPERS/stat_usb \"$SYSFS_PATH\" rc = $STAT_RC"
      else

         # import
         eval "$SOUND_DATA"

         if [ -n "$VDEV_USB_SERIAL" ]; then

            BUS="usb"
            SERIAL="$VDEV_USB_SERIAL"

            if [ -n "$VDEV_USB_INTERFACE_NUM" ]; then

               vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/snd/by-id/$BUS-$SERIAL-$VDEV_USB_INTERFACE_NUM" "$VDEV_METADATA"
            else

               vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/snd/by-id/$BUS-$SERIAL" "$VDEV_METADATA"
            fi
         fi
      fi
   fi

   # set up permissions...
   vdev_permissions $VDEV_VAR_SOUND_OWNER:$VDEV_VAR_SOUND_GROUP $VDEV_VAR_SOUND_MODE "$VDEV_MOUNTPOINT/$VDEV_PATH"

   return 0
}


if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
