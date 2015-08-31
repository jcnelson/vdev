#!/bin/dash

# Subroutines for event processing 

# Enumerate event properties, line by line.
# $1    Device metadata directory; defaults to $VDEV_METADATA if not given 
# Return 0 on success
event_print_properties() {
   
   local _METADATA _LINE
   
   _METADATA="$1"
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi
  
   if [ -f "$_METADATA/properties" ]; then  
      /bin/cat "$_METADATA/properties"
   fi

   return 0
}


# Get a space-separated (!!!) list of device symlinks, and echo the whole thing to stdout.
# $1    Device metadata directory; defaults to $VDEV_METADATA if not given 
# return 0 on success
event_print_symlinks() {

   local _METADATA _DEVLINKS _OLDIFS

   _METADATA="$1"
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if ! [ -f "$_METADATA/links" ]; then 
      return 0
   fi

   _DEVLINKS=""
   _OLDIFS="$IFS"

   while IFS= read -r _LINE; do
      
      _DEVLINKS="$_LINE $_DEVLINKS"

   done < "$_METADATA/links"

   IFS="$_OLDIFS"

   echo "DEVLINKS=$_DEVLINKS"
   return 0
}


# Iterate through the list of tags added for a device, for a device event.
# write a :-separated list of them to stdout
# $1    Device metadata directory; defaults to $VDEV_METADATA if not given 
# Return 0 on success 
event_print_tags() {

   local _TAG _METADATA _TAGS _OLDIFS

   _METADATA="$1"
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if ! [ -d "$_METADATA/tags" ]; then 
      return 0
   fi

   _TAGS=":"
   _OLDIFS="$IFS"
   
   while IFS= read -r _TAG; do 
      
      _TAGS="$_TAGS:$_TAG"
   done <<EOF
$(/bin/ls "$_METADATA/tags")
EOF
   
   IFS="$_OLDIFS"

   echo "$_TAGS:"

   return 0
}


# print the monotonic uptime in microseconds, i.e. for the udev event
# print it to stdout
event_print_monotonic_usec() {
   
   /bin/sed 's/\([0-9]\+\)\.\([0-9]\+\)[ ]\+.*/USEC_INITIALIZED=\1\20000/g' "/proc/uptime"
   return 0
}


# generate the device event text.
# device-specific properties will begin with VDEV_
# device-generic properties include ACTION, DEVPATH, SUBSYSTEM, SEQNUM, DEVNAME, MAJOR, MINOR, IFINDEX, DRIVER, DEVLINKS, TAGS, and USEC_INITIALIZED.
# $1    the action (defaults to $VDEV_ACTION)
# $2    the sysfs device path (defaults to $VDEV_OS_DEVPATH)
# $3    the subsystem name (defaults to $VDEV_OS_SUBSYSTEM)
# $4    the sequence number from the kernel (defaults to $VDEV_OS_SEQNUM)
# $5    the metadata directory for this device (defaults to $VDEV_METADATA)
# Also pulls in VDEV_PATH, VDEV_MAJOR, VDEV_MINOR, VDEV_OS_IFINDEX, VDEV_OS_DRIVER from the caller environment, if they are non-empty
# (VDEV_PATH will be treated as empty if it is "UNKNOWN").
# returns 0 on success
event_generate_text() {

   local _ACTION _SEQNUM _DEVPATH _SUBSYSTEM _METADATA

   _ACTION="$1"
   _DEVPATH="$2"
   _SUBSYSTEM="$3"
   _SEQNUM="$4"
   _METADATA="$5"
   
   if [ -z "$_ACTION" ]; then 
      _ACTION="$VDEV_ACTION"
   fi

   if [ -z "$_DEVPATH" ]; then 
      _DEVPATH="$VDEV_OS_DEVPATH"
   fi

   if [ -z "$_SUBSYSTEM" ]; then 
      _SUBSYSTEM="$VDEV_OS_SUBSYSTEM"
   fi
  
   if [ -z "$_SEQNUM" ]; then 
      _SEQNUM="$VDEV_OS_SEQNUM"
   fi

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi
   
   echo "${_ACTION}@${_DEVPATH}"
   
   echo "ACTION=$_ACTION"
   echo "DEVPATH=$_DEVPATH"
   echo "SUBSYSTEM=$_SUBSYSTEM"
   echo "SEQNUM=$_SEQNUM"

   if [ -n "$VDEV_PATH" ] && [ "$VDEV_PATH" != "UNKNOWN" ]; then 
      echo "DEVNAME=$VDEV_PATH"
   fi

   if [ -n "$VDEV_MAJOR" ]; then 
      echo "MAJOR=$VDEV_MAJOR"
   fi
   
   if [ -n "$VDEV_MINOR" ]; then 
      echo "MINOR=$VDEV_MINOR"
   fi
   
   if [ -n "$VDEV_OS_IFINDEX" ]; then 
      echo "IFINDEX=$VDEV_OS_IFINDEX"
   fi

   if [ -n "$VDEV_OS_DRIVER" ]; then 
      echo "DRIVER=$VDEV_OS_DRIVER"
   fi

   event_print_symlinks "$_METADATA"
   event_print_tags "$_METADATA"
   event_print_properties "$_METADATA"
   event_print_monotonic_usec

   return 0
}
