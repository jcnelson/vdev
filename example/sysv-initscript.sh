#!/bin/sh -e
### BEGIN INIT INFO
# Provides:          udev
# Required-Start:    mountkernfs 
# Required-Stop:     
# Default-Start:     S 
# Default-Stop:      
# Short-Description: Start vdevd, populate /dev and load drivers.
### END INIT INFO

SCRIPTNAME=$0

# fill defaults 
if [ -z "$VDEV_BIN" ]; then 
   VDEV_BIN=/sbin/vdevd
fi

if [ -z "$VDEV_CONFIG" ]; then 
   VDEV_CONFIG=/etc/vdev/vdevd.conf
fi

if [ -z "$VDEV_MOUNTPOINT" ]; then
   VDEV_MOUNTPOINT=/dev
fi


# load an ini file as a set of namespaced environment variables, echoing them to stdout
# $1 is the path to the file 
# $2 is the namespace to be prepended to each variable name 
source_ini_file() {

   local FILE_PATH NAMESPACE line KEY VALUE
   
   FILE_PATH=$1
   NAMESPACE=$2
   # convert [ini style header] to ""
   /bin/cat $FILE_PATH | /bin/sed "s/.*\[.*\].*//g" | \
   while read line; do 
   
      KEY=$(echo $line | /bin/sed "s/\(^.*\)=.*/\1/g");
      VALUE=$(echo $line | /bin/sed "s/^.*=\(.*\)$/\1/g");
      
      if [ -n "$KEY" ]; then 
          echo "${NAMESPACE}${KEY}=${VALUE}"
      fi
   done
}


##############################################################################

[ -x $VDEV_BIN ] || exit 0

PATH="/sbin:/bin"

# defaults
tmpfs_size="10M"

. /lib/lsb/init-functions

# system-wide vdevd needs /sys to be mounted
if [ ! -d /sys/class/ ]; then
  log_failure_msg "vdev requires a mounted sysfs, not started"
  log_end_msg 1
fi

# system-wide vdevd needs "modern" sysfs
if [ -d /sys/class/mem/null -a ! -L /sys/class/mem/null ] || \
   [ -e /sys/block -a ! -e /sys/class/block ]; then
  log_warning_msg "CONFIG_SYSFS_DEPRECATED must not be selected"
  log_warning_msg "Booting will continue in 30 seconds but many things will be broken"
  sleep 30
fi

# load vdev config variables as vdev_config_*
eval $(source_ini_file $VDEV_CONFIG "vdev_config_")

# config sanity check 
if [ -z "$vdev_config_pidfile" ]; then 
   log_warning_msg "No PID file defined in $VDEV_CONFIG.  Please set the pidfile= directive."
   log_warning_msg "You will be unable control vdevd with this init script."
fi


# start up the system-wide vdevd
# $@ arguments to vdevd
vdevd_start() {

   # clear uevent helper--vdevd subsumes its role
   if [ -w /sys/kernel/uevent_helper ]; then
      echo > /sys/kernel/uevent_helper
   fi
   
   # set the SELinux context for devices created in the initramfs
   [ -x /sbin/restorecon ] && /sbin/restorecon -R $VDEV_MOUNTPOINT
   
   log_daemon_msg "Starting the system device event dispatcher" "vdevd"
   
   # make sure log directory exists...
   if [ -n "$vdev_config_logfile" ]; then 
      
      vdev_log_dir="$(echo "$vdev_config_logfile" | sed 's/[^/]\+$//g')"

      if [ -n "$vdev_log_dir" ]; then 
         mkdir -p "$vdev_log_dir"
      fi
   fi

   # make sure the pid directory exists 
   if [ -n "$vdev_config_pidfile" ]; then 

      vdev_pid_dir="$(echo "$vdev_config_pidfile" | sed 's/[^/]\+$//g')"

      if [ -n "$vdev_pid_dir" ]; then 
         mkdir -p "$vdev_pid_dir"
      fi
   fi
   
   # start vdev
   if "$VDEV_BIN" -c "$VDEV_CONFIG" $@ "$VDEV_MOUNTPOINT"; then
       log_end_msg $?
       
   else
       log_warning_msg $?
       log_warning_msg "Errors when starting \"$VDEV_BIN\" -c \"$VDEV_CONFIG\" "
       log_warning_msg "Waiting 15 seconds and trying to continue anyway"
       sleep 15
   fi
   
   return 0
}


# reseed device files with --once
# $@ arguments to vdevd
vdevd_once() {

   log_daemon_msg "(Re)seeding device files" "vdevd"
   
   # run vdevd once
   $VDEV_BIN -c $VDEV_CONFIG --once $@ $VDEV_MOUNTPOINT
   log_end_msg $?
   
   return 0
}


# stop the system-wide vdevd
vdevd_stop() {

   log_daemon_msg "Sending SIGTERM to the system device event dispatcher" "vdevd"

   if ! [ -f $vdev_config_pidfile ]; then 
      echo "No PID file at $vdev_config_pidfile" >&2
      exit 1
   fi

   # kill vdevd with SIGTERM
   VDEV_PID=$(/bin/cat $vdev_config_pidfile)
   /bin/kill -s SIGTERM $VDEV_PID
   
   log_end_msg $?

   return 0
}


case "$1" in
   start)
      
      vdevd_start
      ;;

   stop)

      vdevd_stop 
      ;;

   once)
      
      vdevd_once
      ;;

   restart)
      
      vdevd_stop

      vdevd_start
      ;;

   *)
      echo "Usage: $SCRIPTNAME {start|stop|once|restart}" >&2
      exit 1
      ;;
esac

exit 0

