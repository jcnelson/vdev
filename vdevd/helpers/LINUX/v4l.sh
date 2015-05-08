#!/bin/sh

# video4linux helper 

source $VDEV_HELPERS/subr.sh

# removing? just blow the links away 
if [ "$VDEV_ACTION" = "remove" ]; then 

   vdev_rmlinks $VDEV_METADATA
   exit 0
fi

# must be adding...
if [ "$VDEV_ACTION" != "add" ]; then 

   vdev_fail 10 "Unknown action \'$VDEV_ACTION\'"
fi

BUS=
INDEX=
VENDOR=
MODEL=
TYPE=

ID=
PATH=

# get the type and index
TYPE_PLUS_INDEX=$(echo $VDEV_PATH | /bin/sed -r 's/.*\/([^/]+)/\1/g')

TYPE=$(echo $TYPE_PLUS_INDEX | /bin/sed -r 's/[0-9]+$//g')
INDEX=

if [ -f $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/index ]; then 
   INDEX=$(/bin/cat $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/index)
else 

   # no index? no persistent paths
   exit 0
fi

# is this a USB device?
if [ -n "$(echo $VDEV_OS_DEVPATH | /bin/grep 'usb')" ]; then 

   eval $($VDEV_HELPERS/stat_usb $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH)

   test -n "$VDEV_USB_VENDOR" || exit 1
   test -n "$VDEV_USB_MODEL" || exit 1

   VENDOR=$VDEV_USB_VENDOR
   MODEL=$VDEV_USB_MODEL
   BUS="usb"
fi

# did we get everything to make an ID?
if [ -n "$BUS" -a -n "$INDEX" -a -n "$VENDOR" -a -n "$MODEL" -a -n "$TYPE" ]; then
   
   ID="$BUS-${VENDOR}_${MODEL}-$TYPE-index${INDEX}"
   vdev_symlink ../../$VDEV_PATH $VDEV_MOUNTPOINT/v4l/by-id/$ID $VDEV_METADATA
fi

# by-path 
VDEV_PERSISTENT_PATH=
eval $($VDEV_HELPERS/stat_path $VDEV_MOUNTPOINT/$VDEV_PATH)
RC=$?

[ $RC -ne 0 ] && vdev_fail 1 "$VDEV_HELPERS/stat_path $VDEV_MOUNTPOINT/$VDEV_PATH exit status $RC"

# did we get a path?
[ -z "$VDEV_PERSISTENT_PATH" ] && exit 0

if [ -n "$(echo $VDEV_PATH | /bin/egrep "video|vbi")" ]; then 
   
   # video v4l device 
   vdev_symlink ../../$VDEV_PATH $VDEV_MOUNTPOINT/v4l/by-path/$VDEV_PERSISTENT_PATH-video-index${INDEX} $VDEV_METADATA

elif [ -n "$(echo $VDEV_PATH | /bin/egrep "audio")" ]; then 

   # audio v4l device 
   vdev_symlink ../../$VDEV_PATH $VDEV_MOUNTPOINT/v4l/by-path/$VDEV_PERSISTENT_PATH-audio-index${INDEX} $VDEV_METADATA
fi

# set up permissions...
vdev_permissions root.video 0660 $VDEV_MOUNTPOINT/$VDEV_PATH

exit 0
