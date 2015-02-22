#!/bin/sh

# common subroutines for adding and removing devices 

PROGNAME=$0

# add a device symlink, but remember which device node it was for,
# so we can remove it later even when the device node no longer exists.
# Make all directories leading up to the link as well.
# arguments:
#  $1  link source 
#  $2  link target
#  $3  vdev device metadata directory
add_link() {

   LINK_SOURCE=$1
   LINK_TARGET=$2
   METADATA=$3

   DIRNAME=$(echo $LINK_TARGET | /bin/sed -r "s/[^/]+$//g")

   test -d $DIRNAME || /bin/mkdir -p $DIRNAME

   /bin/ln -s $LINK_SOURCE $LINK_TARGET
   RC=$?

   if [ 0 -eq $RC ]; then

      # save this
      echo $LINK_TARGET >> $METADATA/links
   fi

   return 0
}

# remove all of a device's symlinks, stored by add_link.
# arguments: 
#  $1  vdev device metadata directory
remove_links() {

   METADATA=$1

   while read LINKNAME; do

      DIRNAME=$(echo $LINKNAME | /bin/sed -r "s/[^/]+$//g")
      
      /bin/rm -f $LINKNAME
      /bin/rmdir $DIRNAME 2>/dev/null

   done < $METADATA/links

   /bin/rm -f $METADATA/links
   
   return 0
}


# log a message to the logfile, or stdout 
# arguments:
#   $1  message to log 
vdev_log() {
   
   if [ -z "$VDEV_LOGFILE" ]; then
      # stdout 
      echo "$1" >> /tmp/vdev-subr.log

   else

      # logfile 
      echo "$1" >> $VDEV_LOGFILE

   fi 
}

# log a message to vdev's log and exit 
#   $1 is the exit code 
#   $2 is the (optional) message
fail() {

   CODE=$1
   MSG=$2

   if [ -n "$MSG" ]; then
      vdev_log "$PROGNAME '$VDEV_PATH': $MSG"
   fi

   exit $CODE
}


# print the list of device drivers in a sysfs device path 
#   $1  sysfs device path
drivers() {
   
   SYSFS_PATH=$1

   # strip trailing '/'
   SYSFS_PATH=$(echo $SYSFS_PATH | /bin/sed -r "s/[/]+$//g")
   
   while [ -n "$SYSFS_PATH" ]; do
      
      # driver name is the base path name of the link target of $SYSFS_PATH/driver
      test -L $SYSFS_PATH/driver && /bin/readlink $SYSFS_PATH/driver | /bin/sed -r "s/[^/]*\///g"

      # search parent 
      SYSFS_PATH=$(echo $SYSFS_PATH | /bin/sed -r "s/[^/]+$//g" | /bin/sed -r "s/[/]+$//g")
      
   done
}


# print the list of subsystems in a sysfs device path 
#  $1   sysfs device path 
# NOTE: uniqueness is not guaranteed!
subsystems() {

   SYSFS_PATH=$1

   # strip trailing '/'
   SYSFS_PATH=$(echo $SYSFS_PATH | /bin/sed -r "s/[/]+$//g")
   
   while [ -n "$SYSFS_PATH" ]; do
      
      # subsystem name is the base path name of the link target of $SYSFS_PATH/subsystem
      test -L $SYSFS_PATH/subsystem && /bin/readlink $SYSFS_PATH/subsystem | /bin/sed -r "s/[^/]*\///g"

      # search parent 
      SYSFS_PATH=$(echo $SYSFS_PATH | /bin/sed -r "s/[^/]+$//g" | /bin/sed -r "s/[/]+$//g")
      
   done
}

