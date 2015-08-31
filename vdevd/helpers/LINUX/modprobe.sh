#!/bin/dash

main() {
   /sbin/modprobe -b -q -- "$VDEV_OS_MODALIAS" 
   return 0
}

if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
