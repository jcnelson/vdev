#!/bin/dash

# common subroutines for adding and removing devices 
VDEV_PROGNAME=$0

# add a device symlink, but remember which device node it was for,
# so we can remove it later even when the device node no longer exists.
# Make all directories leading up to the link as well.
# arguments:
#  $1  link source 
#  $2  link target
#  $3  vdev device metadata directory (defaults to $VDEV_METADATA)
vdev_symlink() {

   local _LINK_SOURCE _LINK_TARGET _METADATA _DIRNAME
   
   _LINK_SOURCE="$1"
   _LINK_TARGET="$2"
   _METADATA="$3"

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   _DIRNAME=$(echo $_LINK_TARGET | /bin/sed -r "s/[^/]+$//g")

   test -d $_DIRNAME || /bin/mkdir -p "$_DIRNAME"

   /bin/ln -s "$_LINK_SOURCE" "$_LINK_TARGET"
   _RC=$?

   if [ 0 -eq $_RC ]; then

      # save this
      echo "$_LINK_TARGET" >> "$_METADATA/links"
   fi

   return 0
}

# remove all of a device's symlinks, stored by vdev_symlink.
# preserve the links file itself, though.
# arguments: 
#  $1  vdev device metadata directory
vdev_rmlinks() {

   local _METADATA _LINKNAME _OLDIFS
   
   _METADATA="$1"
   _OLDIFS="$IFS"

   while IFS= read -r _LINKNAME; do

      /bin/rm -f "$_LINKNAME"

   done < "$_METADATA/links"

   _OLDIFS="$IFS"
   
   return 0
}


# log a message to the logfile, or stdout 
# arguments:
#   $1  message to log 
vdev_log() {
   
   if [ -z "$VDEV_LOGFILE" ]; then
      # stdout 
      echo "[helpers] INFO: $1"
   else

      # logfile 
      echo "[helpers] INFO: $1" >> "$VDEV_LOGFILE"
   fi 
}



# log a warning to the logfile, or stdout 
# arguments:
#   $1  message to log 
vdev_warn() {
   
   if [ -z "$VDEV_LOGFILE" ]; then
      # stdout 
      echo "[helpers] WARN: $1"
   else

      # logfile 
      echo "[helpers] WARN: $1" >> "$VDEV_LOGFILE"
   fi 
}


# log an error to the logfile, or stdout 
# arguments:
#   $1  message to log 
vdev_error() {
   
   if [ -z "$VDEV_LOGFILE" ]; then
      # stdout 
      echo "[helpers] ERROR: $1"
   else

      # logfile 
      echo "[helpers] ERROR: $1" >> "$VDEV_LOGFILE"
   fi 
}


# log a message to vdev's log and exit 
#   $1 is the exit code 
#   $2 is the (optional) message
vdev_fail() {

   local _CODE _MSG 
   
   _CODE="$1"
   _MSG="$2"

   if [ -n "$_MSG" ]; then
      vdev_log "$VDEV_PROGNAME '$VDEV_PATH': $_MSG"
   fi

   exit $_CODE
}


# print the list of device drivers in a sysfs device path 
#   $1  sysfs device path
vdev_drivers() {
   
   local _SYSFS_PATH
   
   _SYSFS_PATH="$1"

   # strip trailing '/'
   _SYSFS_PATH="${_SYSFS_PATH%%/}"
   # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[/]+$//g")
   
   while [ -n "$_SYSFS_PATH" ]; do
      
      # driver name is the base path name of the link target of $_SYSFS_PATH/driver
      test -L "$_SYSFS_PATH/driver" && /bin/readlink "$_SYSFS_PATH/driver" | /bin/sed -r "s/[^/]*\///g"

      # search parent 
      # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g" | /bin/sed -r "s/[/]+$//g")
      _SYSFS_PATH="$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g")"
      _SYSFS_PATH="${_SYSFS_PATH%%/}"
      
   done
}


# print the list of subsystems in a sysfs device path 
#  $1   sysfs device path 
# NOTE: uniqueness is not guaranteed!
vdev_subsystems() {

   local _SYSFS_PATH 
   
   _SYSFS_PATH="$1"

   # strip trailing '/'
   _SYSFS_PATH="${_SYSFS_PATH%%/}"
   # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[/]+$//g")

   while [ -n "$_SYSFS_PATH" ]; do
      
      # subsystem name is the base path name of the link target of $_SYSFS_PATH/subsystem
      test -L "$_SYSFS_PATH/subsystem" && /bin/readlink "$_SYSFS_PATH/subsystem" | /bin/sed -r "s/[^/]*\///g"

      # search parent 
      # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g" | /bin/sed -r "s/[/]+$//g")
      _SYSFS_PATH="$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g")"
      _SYSFS_PATH="${_SYSFS_PATH%%/}"
      
   done
}


# generate a device ID, distinct from a path.  Based on udev's scheme
# * if major/minor numbers are given, then this is something like "b259:131072" or "c254:0" (i.e. {type}{major}:{minor})
# * otherwise if a netdev interface number is given, this is something like "n3" (i.e. n{ifnum})
# * otherwise, this is +$subsystem:$sysname
# return 0 and echo the name on success
# return 1 on error
vdev_device_id() {
   
   local _DEVTYPE _DEVICE_ID _SYSNAME

   _DEVTYPE=""
   _DEVICE_ID=""
   _SYSNAME=""
   
   if [ -n "$VDEV_MAJOR" ] && [ -n "$VDEV_MINOR" ]; then 
      
      if [ "$VDEV_MODE" = "block" ]; then 
         _DEVTYPE="b"
      else
         _DEVTYPE="c"
      fi

      _DEVICE_ID="${_DEVTYPE}${VDEV_MAJOR}:${VDEV_MINOR}"
   
   elif [ -n "$VDEV_OS_IFINDEX" ]; then 

      _DEVICE_ID="n${VDEV_OS_IFINDEX}"

   elif [ -n "$VDEV_OS_SUBSYSTEM" ] && [ -n "$VDEV_OS_DEVPATH" ]; then 
      
      _SYSNAME="$(echo "$VDEV_OS_DEVPATH" | /bin/sed -r 's/[^/]*\///g')"
      
      _DEVICE_ID="+${VDEV_OS_SUBSYSTEM}:${_SYSNAME}"
   fi

   if [ -n "$_DEVICE_ID" ]; then 
      echo -n "$_DEVICE_ID"
      return 0
   else 
      return 1
   fi
}


# get the parent device, given a sysfs path.
# this means "the deepest parent with a 'device' symlink"
# $1    the sysfs path of the child 
# return 0 and echo the parent if found 
# return 1 if no parent could be found
vdev_device_parent() {

   local _SYSFS_PATH

   _SYSFS_PATH="$1"

   
   # strip trailing '/'
   # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[/]+$//g")
   _SYSFS_PATH="${_SYSFS_PATH%%/}"
   
   while [ -n "$_SYSFS_PATH" ]; do

      if [ -e "$_SYSFS_PATH/device" ]; then 

         echo "$_SYSFS_PATH"
         return 0
      fi

      
      # search parent 
      # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g" | /bin/sed -r "s/[/]+$//g")
      _SYSFS_PATH="$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g")"
      _SYSFS_PATH="${_SYSFS_PATH%%/}"
      
   done

   return 1
}


# get a device attribute, by looking in the device's sysfs directory
# $1    attr name 
# $2    sysfs device path
# return 0 on success and write the value to stdout 
# return 1 on failure
vdev_getattr() {

   local _SYSFS_PATH _ATTR_NAME _ATTR_VALUE

   _ATTR_NAME="$1"
   _SYSFS_PATH="$2"

   if [ -f "$_SYSFS_PATH/$_ATTR_NAME" ]; then 
      
      _ATTR_VALUE="$(/bin/cat "$_SYSFS_PATH/$_ATTR_NAME")"
      return 0
   fi

   return 1
}


# get all device attributes, by walking the device's sysfs directory and finding the attribute in all its parents 
# $1    attr name 
# $2    sysfs device path
# return 0 on success and write all values to stdout (newline-separated)
# return 1 if no match found
vdev_getattrs() {
   
   local _SYSFS_PATH _ATTR_NAME _ATTR_VALUE

   _ATTR_NAME="$1"
   _SYSFS_PATH="$2"
   _RC=1
   
   # strip trailing '/'
   # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[/]+$//g")
   _SYSFS_PATH="${_SYSFS_PATH%%/}"

   while [ -n "$_SYSFS_PATH" ]; do 

      if [ -f "$_SYSFS_PATH/$_ATTR_NAME" ]; then 
      
         _ATTR_VALUE="$(/bin/cat "$_SYSFS_PATH/$_ATTR_NAME")"
         echo "$_ATTR_VALUE"
      fi

      # search parent 
      # _SYSFS_PATH=$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g" | /bin/sed -r "s/[/]+$//g")
      _SYSFS_PATH="$(echo "$_SYSFS_PATH" | /bin/sed -r "s/[^/]+$//g")"
      _SYSFS_PATH="${_SYSFS_PATH%%/}"
      
      _RC=0
   done

   return $_RC
}


# add a tag for a device.
# create an empty file with the tag name under /dev/metadata/dev/$VDEV_PATH/tags/
# $1   the tag name 
# $2   the device metadata directory.  VDEV_METADATA will be used, if this is not given
# $3   the *global* metadata directory (not the device-specific one).  VDEV_GLOBAL_METADATA will be used, if this is not given.
# $4   the device identifier (see vdev_device_id).  If not given, it will be generated from the environment variables passed in by vdevd.
# return 0 on success 
# return 1 on error
vdev_add_tag() {
   
   local _DEVNAME _TAGNAME _DEVICE_ID _METADATA _GLOBAL_METADATA
   
   _TAGNAME="$1"
   _METADATA="$2"
   _GLOBAL_METADATA="$3"
   _DEVICE_ID=""

   if [ $# -ge 4 ]; then 
      _DEVICE_ID="$4"
   fi

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if [ -z "$_GLOBAL_METADATA" ]; then 
      _GLOBAL_METADATA="$VDEV_GLOBAL_METADATA"
   fi

   if [ -z "$_DEVICE_ID" ]; then 
      _DEVICE_ID="$(vdev_device_id)"
   fi

   # device-to-tags
   if ! [ -e "$_METADATA/tags/$_TAGNAME" ]; then 
      /bin/mkdir -p "$_METADATA/tags"
      echo "" > "$_METADATA/tags/$_TAGNAME"
      _RC=$?
      
      if [ $_RC -ne 0 ]; then 
         return $_RC
      fi 
   fi
   
   return $_RC
}


# add a property for a device, if it is not given yet.
# create it as a file with the property name containing the property value, in the directory /dev/metadata/dev/$VDEV_PATH/properties/
# $1   the property key 
# $2   the property value
# $3   the device metadata directory.  VDEV_METADATA will be used, if this is not given
vdev_add_property() {
   
   local _PROP_KEY _PROP_VALUE _METADATA

   _PROP_KEY="$1"
   _PROP_VALUE="$2"
   _METADATA="$3"
   _RC=0

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   echo "$_PROP_KEY=$_PROP_VALUE" >> "$_METADATA/properties"
   
   return $_RC
}


# add a stream of properties, encoded as positional arguments.
# feed them into vdev_add_property.
# $1    the device-specific metadata directory
# $2+   device properties to grab from the caller's environment
# return 0 on success 
# return 1 if vdev_add_property failed
# return 2 if we failed to parse a KEY=VALUE string
vdev_add_properties() {
   
   local _METADATA _PROP _PROP_VALUE _RC
   
   _METADATA="$1"
   _RC=0

   shift 1
   
   while [ $# -gt 0 ]; do

      _PROP="$1"
      shift 1

      _PROP_VALUE=
      eval "_PROP_VALUE=\"\${$_PROP}\""

      if [ -z "$_PROP_VALUE" ]; then 
         continue 
      fi
      
      vdev_add_property "$_PROP" "$_PROP_VALUE" "$_METADATA"
      _RC=$?
      
      if [ $_RC -ne 0 ]; then 
         break
      fi
   
   done 
   
   return $_RC
}


# set permissions and ownership on a device 
# do not change permissions if the owner/group isn't defined 
# $1    the "owner.group" string, to be fed into chmod 
# $2    the (octal) permissions, to be fed into chown
# $3    the device path 
# return 0 on success
# return 1 on failure 
vdev_permissions() {

   local _OWNER _MODE _PATH _RC _CHOWN _CHMOD
   
   _OWNER="$1"
   _MODE="$2"
   _PATH="$3"

   _CHOWN="/bin/chown"
   _CHMOD="/bin/chmod"

   $_CHOWN $_OWNER $_PATH
   _RC=$?

   if [ $_RC -ne 0 ]; then 
      return $_RC
   fi

   $_CHMOD "$_MODE" "$_PATH"
   _RC=$?

   return $_RC
}


# serialize a path: swap each / with \x2f
# $1    the path 
# echo the serialized path to stdout 
# return 0 on success
# return 1 on error 
vdev_serialize_path() {

   local _PATH 
   
   _PATH="$1"

   echo "$_PATH" | /bin/sed -r 's/\//\\x2f/g'
}


# clean up a device's metadata 
#  $1   vdev device metadata directory
# return 0 on success
vdev_cleanup() {
   
   local _METADATA 

   _METADATA="$1"
   
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   vdev_rmlinks "$_METADATA"
   return 0
}


# unset a list of environment variables--i.e. set them to empty
# all positional arguments are their names 
# return 0 on success 
vdev_unset() {

   local _ENV_NAME

   while [ $# -gt 0 ]; do

      _ENV_NAME="$1"
      shift 1

      eval "$_ENV_NAME=\"\""
   done

   return 0
}


# helper wrapper around blkid, since busybox's blkid has different 
# options than util-linux's blkid.
# prints out environment variables like util-linux's "-p -o export" options, which the caller can eval.
# $1 path to the device node 
# return blkid's status on success.
vdev_blkid() {

   local _PATH _VARS _OLDIFS _VARENT _EXPR _RC
   _RC=0

   _PATH="$1"

   # if /sbin/blkid is a symlink, assume it's busybox 
   if [ -L "/sbin/blkid" ]; then 

      _VARS="$(/sbin/blkid "$_PATH")"
      _RC=$?

      if [ $_RC -ne 0 ]; then 
         return $_RC
      fi

      # output format includes "/dev/XXX: ".  strip this.
      _VARS=${_VARS##$_PATH: }

      # consume each variable.
      # values are always quoted.
      while [ ${#_VARS} -gt 0 ]; do

         _VARENT="$(echo "$_VARS" | /bin/sed -r 's/^([^ =]+)="([^"]+)" .+$/\1="\2"/g')"

         _EXPR="s/$_VARENT//g"

         _VARENT=${_VARENT## }
         
         _VARS="$(echo "$_VARS" | /bin/sed -r "$_EXPR")"

         echo "$_VARENT"
      done

   else

      # assume it's util-linux's blkid 
      /sbin/blkid -p -o export "$_PATH"
      _RC=$?
   fi

   return $_RC
}
