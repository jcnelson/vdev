#!/bin/sh

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
INDEX=$(/bin/cat $VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH/index)

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
fi

# TODO: by-path

case "$VDEV_ACTION" in 

   add)

      /bin/mkdir -p $VDEV_MOUNTPOINT/v4l/by-id
      /bin/ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/v4l/by-id/$ID
      ;;

   remove)

      /bin/rm -f $VDEV_MOUNTPOINT/v4l/by-id/$ID
      ;;

   test)

      echo "ln -s ../../$VDEV_PATH $VDEV_MOUNTPOINT/v4l/by-id/$ID"
      ;;

esac

exit 0
