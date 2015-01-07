#!/bin/sh

# if we have a bus and device number, use them
if [ -n "$VDEV_OS_BUSNUM" -a -n "$VDEV_OS_DEVNUM" ]; then 

   # USB bus device
   echo -n "bus/usb/$VDEV_OS_BUSNUM/$VDEV_OS_DEVNUM"

else

   # some other USB device
   # TODO: need other handlers
   echo -n $VDEV_PATH
fi


exit 0
