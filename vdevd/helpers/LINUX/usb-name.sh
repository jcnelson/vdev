#!/bin/dash

# only deal with devices we know exist
if [ "$VDEV_PATH" = "UNKNOWN" ]; then 
   exit 0
fi

# if we have a bus and device number, use them
if [ -n "$VDEV_OS_BUSNUM" -a -n "$VDEV_OS_DEVNUM" ]; then 

   # USB bus device
   echo -n "bus/usb/$VDEV_OS_BUSNUM/$VDEV_OS_DEVNUM"
fi

exit 0
