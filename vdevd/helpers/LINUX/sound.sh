#!/bin/sh

# set up /dev/snd/by-path and /dev/snd/by-id 

. $VDEV_HELPERS/subr.sh

# if removing, just remove symlinks
if [ "$VDEV_ACTION" = "remove" ]; then 
   
   vdev_rmlinks $VDEV_METADATA
fi

# make sure we're adding 
if [ "$VDEV_ACTION" != "add" ]; then 
   
   vdev_fail 10 "Unknown action '$VDEV_ACTION'"
fi

# if we're dealing with ALSA controlC[0-9] files, set up persistent paths 
if [ -n "$(echo $VDEV_PATH | /bin/egrep "controlC[0-9]+")" ]; then 

   eval $($VDEV_HELPERS/stat_path $VDEV_MOUNTPOINT/$VDEV_PATH)
   STAT_RC=$?

   # verify that we got a persistent path 
   test 0 -ne $STAT_RC && exit 0
   test -z "$VDEV_PERSISTENT_PATH" && exit 0

   # install the path 
   vdev_symlink ../../$VDEV_PATH $VDEV_MOUNTPOINT/snd/by-path/$VDEV_PERSISTENT_PATH $VDEV_METADATA
fi

# set up permissions...
vdev_permissions root.audio 0660 $VDEV_MOUNTPOINT/$VDEV_PATH

exit 0
