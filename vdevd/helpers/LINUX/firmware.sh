#!/bin/sh

. $VDEV_HELPERS/subr.sh

# if instructed to load firmware, do so
if [ -n "$VDEV_OS_FIRMWARE" -a -n "$VDEV_OS_DEVPATH" ]; then 
   
   vdev_firmware_load $VDEV_OS_DEVPATH $VDEV_FIRMARE_DIR/$VDEV_OS_FIRMWARE 
   
   _RC=$?

   if [ $_RC -ne 0 ]; then 
      vdev_error "Failed to load formware '$VDEV_FIRMARE_DIR/$VDEV_OS_FIRMWARE' for '$VDEV_OS_DEVPATH'"
   fi
   
fi

exit 0