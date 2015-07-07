#!/bin/sh

# Helper to push device events to listening processes.

. "$VDEV_HELPERS/subr-event.sh"

# entry point 
main() {

   # require sequence number and devpath, at least 
   if [ -z "$VDEV_OS_SEQNUM" ] || [ -z "$VDEV_OS_DEVPATH" ] || [ -z "$VDEV_OS_SUBSYSTEM" ]; then 
      return 0
   fi

   # clear path if UNKNOWN
   if [ "$VDEV_PATH" = "UNKNOWN" ]; then 
      VDEV_PATH=
   fi

   # propagate to each event queue
   "$VDEV_HELPERS/event-put" -s "$VDEV_MOUNTPOINT/events/global" 4<<EOF
$(event_generate_text "$VDEV_ACTION" "$VDEV_OS_DEVPATH" "$VDEV_OS_SUBSYSTEM" "$VDEV_OS_SEQNUM" "$VDEV_METADATA")
EOF
   return $?
}

if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi