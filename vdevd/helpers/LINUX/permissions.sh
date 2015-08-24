#!/bin/dash

# vdevd helper to set permissions and ownership.
# takes the owner from the VDEV_VAR_PERMISSIONS_OWNER environment variable.
# takes the group from the VDEV_VAR_PERMISSIONS_GROUP environment variable.
# takes the mode from the VDEV_VAR_PERMISSIONS_MODE environment variable

# entry point 
main() {
    
    if [ -n "$VDEV_VAR_PERMISSIONS_OWNER" ] && [ -n "$VDEV_VAR_PERMISSIONS_GROUP" ]; then 

        /bin/chown "$VDEV_VAR_PERMISSIONS_OWNER":"$VDEV_VAR_PERMISSIONS_GROUP" "$VDEV_MOUNTPOINT/$VDEV_PATH"
    elif [ -n "$VDEV_VAR_PERMISSIONS_OWNER" ]; then 
        
        /bin/chown "$VDEV_VAR_PERMISSIONS_OWNER" "$VDEV_MOUNTPOINT/$VDEV_PATH"
    elif [ -n "$VDEV_VAR_PERMISSIONS_GROUP" ]; then 
        
        /bin/chgrp "$VDEV_VAR_PERMISSIONS_GROUP" "$VDEV_MOUNTPOINT/$VDEV_PATH"
    fi 

    if [ -n "$VDEV_VAR_PERMISSIONS_MODE" ]; then 
        
        /bin/chmod "$VDEV_VAR_PERMISSIONS_MODE" "$VDEV_MOUNTPOINT/$VDEV_PATH"
    fi

    # reset for next time 
    VDEV_VAR_PERMISSIONS_GROUP=
    VDEV_VAR_PERMISSIONS_OWNER=
    VDEV_VAR_PERMISSIONS_MODE=
    
    return 0
}

if [ $VDEV_DAEMONLET -eq 0 ]; then 
   
   set +u
   main 
   exit $?
else 
   
   # initialize  
   set -u
   VDEV_VAR_PERMISSIONS_GROUP=
   VDEV_VAR_PERMISSIONS_OWNER=
   VDEV_VAR_PERMISSIONS_MODE=
fi
