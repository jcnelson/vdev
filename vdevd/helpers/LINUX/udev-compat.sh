#!/bin/dash

# Helper to maintain udev compatibility.
# This should be run last, once the device has been initialized

. "$VDEV_HELPERS/subr.sh"


# Determine whether or not a string is prefixed with a given substring.
# $1    the prefix
# $2    the string in question
# Return 0 if so
# Return 1 if not
has_prefix() {
   
   local _PREFIX _STRING _STRTMP _LEN1 _LEN2

   _PREFIX="$1"
   _STRING="$2"

   _LEN1=${#_STRING}
   
   _STRTMP=${_STRING##$_PREFIX}

   _LEN2=${#_STRTMP}

   if [ $_LEN1 -eq $_LEN2 ]; then 
      # prefix was not chomped
      # this isn't a prefix 
      return 1
   else
      return 0
   fi
}


# Iterate through the list of tags added for a device.
# Write them line by line to stdout.
# $1    Device metadata directory; defaults to $VDEV_METADATA if not given 
# Return 0 on success 
enumerate_tags() {

   local _TAG _METADATA

   _METADATA="$1"
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if ! [ -d "$_METADATA/tags" ]; then 
      return 0
   fi

   while read -r _TAG; do 
      
      echo "$_TAG"
   done <<EOF
$(/bin/ls "$_METADATA/tags")
EOF

   return 0
}


# Enumerate udev properties.  This writes the "E" records in the device's equivalent of /run/udev/data/$DEVICE_ID to stdout.
# $1    Device metadata directory; defaults to $VDEV_METADATA if not given 
# Return 0 on success
enumerate_udev_properties() {
   
   local _METADATA
   
   _METADATA="$1"
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   /bin/cat "$_METADATA/properties" | \
      /bin/sed -r -e 's/^VDEV_ATA/E:ID_ATA/g' | \
      /bin/sed -r -e 's/^VDEV_BUS/E:ID_BUS/g' | \
      /bin/sed -r -e 's/^VDEV_CCISS/E:ID_CCISS/g' | \
      /bin/sed -r -e 's/^VDEV_DM/E:DM/g' | \
      /bin/sed -r -e 's/^VDEV_FS/E:ID_FS/g' | \
      /bin/sed -r -e 's/^VDEV_INPUT/E:ID_INPUT/g' | \
      /bin/sed -r -e 's/^VDEV_MAJOR/E:MAJOR/g' | \
      /bin/sed -r -e 's/^VDEV_MINOR/E:MINOR/g' | \
      /bin/sed -r -e 's/^VDEV_NET/E:ID_NET/g' | \
      /bin/sed -r -e 's/^VDEV_OPTICAL/E:ID_CDROM/g' | \
      /bin/sed -r -e 's/^VDEV_OS_/E:/g' | \
      /bin/sed -r -e 's/^VDEV_PART_ENTRY/E:ID_PART_ENTRY/g' | \
      /bin/sed -r -e 's/^VDEV_PART_TABLE/E:ID_PART_TABLE/g' | \
      /bin/sed -r -e 's/^VDEV_PERSISTENT_PATH/E:ID_PATH/g' | \
      /bin/sed -r -e 's/^VDEV_REVISION/E:ID_REVISION/g' | \
      /bin/sed -r -e 's/^VDEV_SCSI/E:ID_SCSI/g' | \
      /bin/sed -r -e 's/^VDEV_SERIAL/E:ID_SERIAL/g' | \
      /bin/sed -r -e 's/^VDEV_USB/E:ID_USB/g' | \
      /bin/sed -r -e 's/^VDEV_TYPE/E:ID_TYPE/g' | \
      /bin/sed -r -e 's/^VDEV_V4L/E:ID_V4L/g' | \
      /bin/sed -r -e 's/^VDEV_WWN/E:ID_WWN/g' | \
      /bin/sed -r -e 's/^XKB/E:XKB/g' | \
      /bin/sed -r -e 's/^BACKSPACE/E:BACKSPACE/g' | \
      /bin/sed -r -e 's/^KMAP/E:KMAP/g'
   
   return 0
}


# Enumerate udev symlinks.  This writes the "S" records in the device's equivalent of /run/udev/data/$DEVICE_ID to stdout.
# $1    Device metadata directory: defaults to $VDEV_METADATA if not given 
# $2    /dev mountpoint; defaults to $VDEV_MOUNTPOINT
# Returns 0 on success
enumerate_udev_symlinks() {
   
   local _METADATA _MOUNTPOINT _LINE _STRIPPED_LINE

   _METADATA="$1"
   _MOUNTPOINT="$2"
   
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if [ -z "$_MOUNTPOINT" ]; then 
      _MOUNTPOINT="$VDEV_MOUNTPOINT"
   fi

   if ! [ -f "$_METADATA/links" ]; then 
      return 0
   fi

   while read _LINE; do
   
      _STRIPPED_LINE="${_LINE##$_MOUNTPOINT}"
      _STRIPPED_LINE="${_STRIPPED_LINE##/}"
      
      echo "S:${_STRIPPED_LINE}"
      
   done < "$_METADATA/links"


   return 0
}


# Enumerate udev tags.  This writes the "G" records in the device's equivalent of /run/udev/data/$DEVICE_ID to stdout.
# $1    Device metadata directory; defaults to $VDEV_METADATA if not given 
# Returns 0 on success 
enumerate_udev_tags() {

   local _METADATA _LINE
   
   _METADATA="$1"
   
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   enumerate_tags "$_METADATA" | \
   while read _LINE; do 
      
      echo "G:$_LINE"
   done 

   return 0
}


# Get the current monotonic uptime in microseconds, i.e. for udev compatibility.
# Print it to stdout--it's too big to return.
now_monotonic_usec() {
   
   local _UPTIME_SEC _UPTIME_USEC
   
   _UPTIME_USEC="$(/bin/cat "/proc/uptime" | /bin/sed -r 's/([0-9]+)\.([0-9]+)[ ]+.*/\1\20000/g')"

   if [ -z "$_UPTIME_USEC" ]; then 
      echo "0"
   else
      echo "$_UPTIME_USEC"
   fi

   return 0
}

# Generate a udev-compatible device database record, i.e. the file under /run/udev/data/$DEVICE_ID.
# It will be stored under /dev/metadata/udev/data/$DEVICE_ID, which in turn can be symlinked to /run/udev
# $1    Device ID (defaults to the result of vdev_device_id)
# $2    Device metadata directory (defaults to $VDEV_METADATA)
# $3    Global metadata directory (defaults to $VDEV_GLOBAL_METADATA)
# $4    device hierarchy mountpoint (defaults to $VDEV_MOUNTPOINT)
# NOTE: the /dev/metadata/udev directory hierarchy must have been set up (e.g. by dev-setup.sh)
# return 0 on success, and generate /dev/metadata/udev/data/$DEVICE_ID to contain the same information that /run/udev/data/$DEVICE_ID would contain
# return non-zero on error
generate_udev_data() {

   local _DEVICE_ID _METADATA _GLOBAL_METADATA _MOUNTPOINT _UDEV_DATA_PATH _UDEV_DATA_PATH_TMP _RC _INIT_TIME_USEC
   
   _DEVICE_ID="$1"
   _METADATA="$2"
   _GLOBAL_METADATA="$3"
   _MOUNTPOINT="$4"

   if [ -z "$_DEVICE_ID" ]; then 
      _DEVICE_ID="$(vdev_device_id)"
   fi
   
   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if [ -z "$_GLOBAL_METADATA" ]; then 
      _GLOBAL_METADATA="$VDEV_GLOBAL_METADATA"
   fi

   if [ -z "$_MOUNTPOINT" ]; then
      _MOUNTPOINT="$VDEV_MOUNTPOINT"
   fi
   
   _UDEV_DATA_PATH="$_GLOBAL_METADATA/udev/data/$_DEVICE_ID"
   _UDEV_DATA_PATH_TMP="$_GLOBAL_METADATA/udev/data/.$_DEVICE_ID.tmp"
   
   enumerate_udev_symlinks "$_METADATA" "$_MOUNTPOINT" >> "$_UDEV_DATA_PATH_TMP"
   _RC=$?

   if [ $_RC -ne 0 ]; then 

      /bin/rm -f "$_UDEV_DATA_PATH_TMP"
      vdev_error "enumerate_udev_symlinks rc = $_RC"
      return $_RC
   fi 

   _INIT_TIME_USEC="$(now_monotonic_usec)"
   echo "I:$_INIT_TIME_USEC" >> "$_UDEV_DATA_PATH_TMP"
   _RC=$?

   if [ $_RC -ne 0 ]; then 

      /bin/rm -f "$_UDEV_DATA_PATH_TMP"
      vdev_error "now_monotonic_usec rc = $_RC"
      return $_RC
   fi 

   enumerate_udev_properties "$_METADATA" >> "$_UDEV_DATA_PATH_TMP"
   _RC=$?

   if [ $_RC -ne 0 ]; then 

      /bin/rm -f "$_UDEV_DATA_PATH_TMP"
      vdev_error "enumerate_udev_properties rc = $_RC"
      return $_RC
   fi 

   enumerate_udev_tags "$_METADATA" >> "$_UDEV_DATA_PATH_TMP"
   _RC=$?

   if [ $_RC -ne 0 ]; then 
      
      /bin/rm -f "$_UDEV_DATA_PATH_TMP"
      vdev_error "enumerate_udev_tags rc = $_RC"
      return $_RC
   fi 

   /bin/mv "$_UDEV_DATA_PATH_TMP" "$_UDEV_DATA_PATH"
   return $?
}


# remove udev data, from /dev/metadata/udev/data/$DEVICE_ID 
# $1    The device ID (defaults to the string generated by vdev_device_id)
# $2    The global metadata directory (defaults to $VDEV_GLOBAL_METADATA)
# return 0 on success
# return nonzero on error
remove_udev_data() {
   
   local _DEVICE_ID _GLOBAL_METADATA
   
   _DEVICE_ID="$1"
   _GLOBAL_METADATA="$2"

   if [ -z "$_DEVICE_ID" ]; then 
      _DEVICE_ID="$(vdev_device_id)"
   fi
   
   if [ -z "$_GLOBAL_METADATA" ]; then 
      _GLOBAL_METADATA="$VDEV_GLOBAL_METADATA"
   fi

   _UDEV_DATA_PATH="$_GLOBAL_METADATA/udev/data/$_DEVICE_ID"

   /bin/rm -f "$_UDEV_DATA_PATH"
   return $?
}


# Generate a udev-compatible symlinks index, i.e. the device-specific directories under /run/udev/links/ that contain this given device.
# For each symlink, create a directory in /dev/metadata/udev/links that contains the serialized links path relative to the mountpoint, and put 
# the udev device ID within that directory.  For example, /dev/disk/by-id/ata-MATSHITADVD-RAM_UJ890_UG99_083452 gets a directory 
# called /dev/metadata/udev/links/\x2fdisk\x2fby-id\x2fata-MATSHITADVD-RAM_UJ890_UG99_083452, and a file named after the device ID (e.g. b8:0) 
# gets created within it.
# $1    The device ID (defaults to the string generated by vdev_device_id)
# $2    The device metadata hierarchy (defaults to $VDEV_METADATA)
# $3    The global metadata hierarchy (defaults to $VDEV_GLOBAL_METADATA)
# $4    The device hierarchy mountpoint (defaults to $VDEV_MOUNTPOINT)
# NOTE: the /dev/metadata/udev directory hierarchy must have been set up (e.g. by dev-setup.sh)
# return 0 on success 
# return nonzero on error 
generate_udev_links() {

   local _RC _DEVICE_ID _METADATA _GLOBAL_METADATA _MOUNTPOINT _LINE _STRIPPED_LINE _LINK_DIR

   _DEVICE_ID="$1"
   _METADATA="$2"
   _GLOBAL_METADATA="$3"
   _MOUNTPOINT="$4"

   if [ -z "$_DEVICE_ID" ]; then 
      _DEVICE_ID="$(vdev_device_id)"
   fi

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if [ -z "$_GLOBAL_METADATA" ]; then 
      _GLOBAL_METADATA="$VDEV_GLOBAL_METADATA"
   fi
   
   if [ -z "$_MOUNTPOINT" ]; then 
      _MOUNTPOINT="$VDEV_MOUNTPOINT"
   fi

   if ! [ -f "$_METADATA/links" ]; then 
      return 0
   fi
   
   while read _LINE; do
   
      _STRIPPED_LINE="${_LINE##$_MOUNTPOINT}"
      
      _LINK_DIR="$(vdev_serialize_path "$_STRIPPED_LINE")"

      if [ -z "$_LINK_DIR" ]; then 
         vdev_warn "Empty link: $_LINE"
         continue 
      fi

      # expand
      _LINK_DIR="$_GLOBAL_METADATA/udev/links/$_LINK_DIR"
      
      if ! [ -d "$_LINK_DIR" ]; then 

         /bin/mkdir -p "$_LINK_DIR"
         _RC=$?

         if [ $_RC -ne 0 ]; then 
            
            vdev_error "mkdir $_LINK_DIR failed"
            break
         fi
      
      else

         # TODO: is it possible for there to be duplicate symlinks anymore?  Old udev bug reports suggest as much.
         vdev_warn "Duplicate symlink for $_DEVICE_ID"
      fi

      echo "" > "$_LINK_DIR/$_DEVICE_ID"
      RC=$?

      if [ $_RC -ne 0 ]; then 

         vdev_error "Indexing link at $_LINK_DIR/$_DEVICE_ID failed"
         break
      fi 
      
   done < "$_METADATA/links"

   return $_RC
}


# remove the udev symlink reverse index
# $1    The device ID (defaults to the string generated by vdev_device_id)
# $2    The device metadata directory (defaults to $VDEV_METADATA)
# $3    The device hierarchy mountpoint (defaults to $VDEV_MOUNTPOINT)
# return 0 on success 
# return nonzero on error
remove_udev_links() {
   
   local _RC _DEVICE_ID _MOUNTPOINT _LINE _STRIPPED_LINE _LINK_DIR _METADATA

   _DEVICE_ID="$1"
   _METADATA="$2"
   _MOUNTPOINT="$3"

   if [ -z "$_DEVICE_ID" ]; then 
      _DEVICE_ID="$(vdev_device_id)"
   fi

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi
   
   if [ -z "$_MOUNTPOINT" ]; then 
      _MOUNTPOINT="$VDEV_MOUNTPOINT"
   fi
   
   
   if ! [ -f "$_METADATA/links" ]; then 
      return 0
   fi

   _UDEV_DATA_PATH="$_METADATA/udev/data/$_DEVICE_ID"

   /bin/cat "$_METADATA/links" | \
   while read _LINE; do
   
      _STRIPPED_LINE="${_LINE##$_MOUNTPOINT}"
      
      _LINK_DIR="$(echo "$_STRIPPED_LINE" | /bin/sed -r 's/\//\\x2f/g')"

      if [ -z "$_LINK_DIR" ]; then 
         vdev_warn "Empty link: $_LINE"
         continue 
      fi

      if ! [ -d "$_LINK_DIR" ]; then 
         continue
      else

         /bin/rm -f "$_LINK_DIR/$_DEVICE_ID"
         /bin/rmdir -f "$_LINK_DIR"
         
         # TODO: is it possible for there to be duplicate symlinks anymore?  Old udev bug reports suggest as much.
         if [ $? -ne 0 ]; then 
            vdev_warn "Link index directory not empty: $_LINK_DIR"
         fi
      fi

   done

   return 0
}


# Generate a udev-compatible tags index, i.e. the tag-to-device index under /run/udev/tags that contain this given device.
# For each tag, touch an empty file in /dev/metadata/udev/tags/$TAG/$DEVICE_ID.
# NOTE: the /dev/metadata/udev directory hierarchy must have been set up (e.g. by dev-setup.sh)
# $1    Device ID (defaults to the result of vdev_device_id)
# $2    device metadata directory (defaults to $VDEV_METADATA)
# $2    global metadata directory (defaults to $VDEV_GLOBAL_METADATA)
# return 0 on success 
# return nonzero on error 
generate_udev_tags() {
   
   local _RC _DEVICE_ID _METADATA _GLOBAL_METADATA _LINE _TAGDIR

   _DEVICE_ID="$1"
   _METADATA="$2"
   _GLOBAL_METADATA="$3"
   
   if [ -z "$_DEVICE_ID" ]; then 
      _DEVICE_ID="$(vdev_device_id)"
   fi

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   if [ -z "$_GLOBAL_METADATA" ]; then 
      _GLOBAL_METADATA="$VDEV_GLOBAL_METADATA"
   fi

   enumerate_tags "$_METADATA" | \
   while read _LINE; do 
      
      _TAGDIR="$_GLOBAL_METADATA/udev/tags/$_LINE"
       
      if ! [ -d "$_TAGDIR" ]; then 

         /bin/mkdir -p "$_TAGDIR"
         _RC=$?

         if [ $_RC -ne 0 ]; then 

            vdev_warn "mkdir $_TAGDIR failed"
            break
         fi
      fi

      echo "" > "$_TAGDIR/$_DEVICE_ID"
      RC=$?

      if [ $_RC -ne 0 ]; then 

         vdev_warn "create $_TAGDIR/$_DEVICE_ID failed"
         break
      fi
   done
   
   return $_RC
}


# Remove udev tags for this device 
# $1    Device ID (defaults to the result of vdev_device_id)
# $2    Device metadata directory (defaults to $VDEV_METADATA)
# return 0 on success
remove_udev_tags() {
   
   local _RC _DEVICE_ID _METADATA _LINE _TAGDIR

   _DEVICE_ID="$1"
   _METADATA="$2"
   
   if [ -z "$_DEVICE_ID" ]; then 
      _DEVICE_ID="$(vdev_device_id)"
   fi

   if [ -z "$_METADATA" ]; then 
      _METADATA="$VDEV_METADATA"
   fi

   enumerate_tags "$_METADATA" | \
   while read _LINE; do 
      
      _TAGDIR="$_METADATA/udev/tags"
       
      if ! [ -d "$_TAGDIR" ]; then 
         continue 
      else

         /bin/rm -f "$_TAGDIR/$_DEVICE_ID"
         /bin/rmdir "$_TAGDIR" || true
      fi
   done
   
   return 0
}


# get the hwdb prefix from an input class 
# $1    the input class 
input_class_to_hwdb_prefix() {

   local _INPUT_CLASS _HWDB_PREFIX

   _INPUT_CLASS="$1"

   case $_INPUT_CLASS in 
      
      kbd)
         
         _HWDB_PREFIX="keyboard"
         ;;

      mouse|joystick)

         _HWDB_PREFIX="mouse"
         ;;

      *)
         
         _HWDB_PREFIX="$_INPUT_CLASS"
         ;;
   esac

   echo "$_HWDB_PREFIX"
   return 0
}


# use the hwdb to store extra properties for this device 
# $1    Path to the hardware database root directory 
# return 0 on success
add_hwdb_properties() {

   local _HWDB_PATH 

   _HWDB_PATH="$1"

   HWDB_ARGS="-h $_HWDB_PATH"
   
   if [ -n "$VDEV_OS_MODALIAS" ]; then 
      
      if [ "$VDEV_OS_SUBSYSTEM" = "input" ]; then 
         
         # use the input class as the prefix 
         INPUT_CLASS="$(/bin/cat "$VDEV_METADATA/properties" | /bin/grep "VDEV_INPUT_CLASS=" | /bin/sed -r 's/VDEV_INPUT_CLASS=//g')"

         if [ -n "$INPUT_CLASS" ]; then 
            PREFIX="$(input_class_to_hwdb_prefix "$INPUT_CLASS")"

            HWDB_ARGS="$HWDB_ARGS -P $PREFIX"
         fi
      fi

      HWDB_ARGS="$HWDB_ARGS $VDEV_OS_MODALIAS"
   
   elif [ -n "$VDEV_OS_DEVPATH" ]; then 
      
      if [ "$VDEV_OS_SUBSYSTEM" = "usb" ] || [ "$VDEV_OS_SUBSYSTEM" = "usb_device" ]; then 
         
         HWDB_ARGS="$HWDB_ARGS -D $VDEV_OS_DEVPATH -S $VDEV_OS_SUBSYSTEM"
      
      else

         # not enough info to search the hwdb
         return 0
      fi
   
   else
   
      # not enough info to search the hwdb
      return 0
   fi
   
   if [ -n "$HWDB_ARGS" ]; then 

      while read -r PROP_NAME_AND_VALUE; do 
         
         OLDIFS="$IFS"
         IFS="="
         
         set -- "$PROP_NAME_AND_VALUE"
         
         PROP_NAME="$1"
         PROP_VALUE="$2"
         
         IFS="$OLD_IFS"

         if [ -n "$PROP_NAME" ] && [ -n "$PROP_VALUE" ]; then 

            # put the property
            vdev_add_property "$PROP_NAME" "$PROP_VALUE" "$VDEV_METADATA"
         fi
         
      done <<EOF
$("$VDEV_HELPERS/hwdb.sh" $HWDB_ARGS)
EOF
   fi

   return 0
}


# entry point
# if our path is still "UNKNOWN", then generate a device ID and go with that 
if [ "$VDEV_PATH" = "UNKNOWN" ]; then 
   
   VDEV_METADATA="$VDEV_GLOBAL_METADATA/dev"/"$(vdev_device_id)"
   /bin/mkdir -p "$VDEV_METADATA"
fi

if [ "$VDEV_ACTION" = "add" ]; then 

   VDEV_HWDB_PATH="$VDEV_MOUNTPOINT/metadata/hwdb"

   # insert extra properties, if we have a hwdb
   if [ -d "$VDEV_HWDB_PATH" ]; then 
      
      add_hwdb_properties "$VDEV_HWDB_PATH"
   else 

      vdev_warn "Could not find hardware database ($VDEV_HWDB_PATH)"
   fi

   # add udev data
   generate_udev_data || vdev_fail 10 "Failed to generate udev data for $VDEV_PATH"
   generate_udev_links || vdev_fail 11 "Failed to generate udev links for $VDEV_PATH"
   generate_udev_tags || vdev_fail 12 "Failed to generate udev tags for $VDEV_PATH"

elif [ "$VDEV_ACTION" = "remove" ]; then 

   # remove udev data 
   remove_udev_data 
   remove_udev_links
   remove_udev_tags
fi

exit 0
