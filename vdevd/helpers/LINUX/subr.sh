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

      echo "remove $LINKNAME" >> /tmp/vdev-subr.log 

      /bin/rm -f $LINKNAME
      /bin/rmdir $DIRNAME 2>/dev/null

   done < $METADATA/links

   /bin/rm -f $METADATA/links
   
   return 0
}

# log a message to vdev's log and exit 
#   $1 is the exit code 
#   $2 is the (optional) message
fail() {

   CODE=$1
   MSG=$2

   if [ -n "$MSG" ]; then
      echo "$PROGNAME \'$VDEV_PATH\': $MSG" >> $VDEV_LOGFILE
   fi

   exit $CODE
}

