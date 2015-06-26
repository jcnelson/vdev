#!/bin/dash

# rename interfaces to have persistnet names, according to the rules in $VDEV_IFNAMES_PATH
# the format of this file should be lines of:
# $IFNAME $ID_TYPE $ID_ARG
# where $IFNAME is the persistent name; $ID_TYPE is either "mac" or "devpath", and
# $ID_ARG is either the MAC address (formatted xx:xx:xx:xx:xx:xx) if $ID_TYPE is "mac", or
# $ID_ARG is the device path (starting with /devices)

. "$VDEV_HELPERS/subr.sh"

# go find the ifnames file from our config
IFNAMES_VAR="$(/bin/fgrep "ifnames=" "$VDEV_CONFIG_FILE")"
VDEV_IFNAMES_PATH=

if [ -n "$IFNAMES_VAR" ]; then 
   eval "$IFNAMES_VAR"
   VDEV_HWDB_PATH="$ifnames"
fi


# make sure the file exists
test -e "$VDEV_IFNAMES_PATH" || exit 0
test -f "$VDEV_IFNAMES_PATH" || vdev_fail 1 "Could not find ifnames file $VDEV_IFNAMES_PATH"

# only bother with adding 
if [ "$VDEV_ACTION" = "remove" ]; then 
   exit 0
fi

IP=

if [ -x /sbin/ip ]; then 
   IP=/sbin/ip
elif [ -x /bin/ip ]; then 
   IP=/bin/ip
fi

if [ -z "$IP" ]; then 
   vdev_fail 0 "Could not find iproute2.  Network interfaces will not be configured."
fi


# rename an interface 
# $1    desired interface name 
# $2    original interface name 
rename_if() {
   
   local _IFNAME
   local _IFNAME_ORIG 
   local _IF_IS_DOWN 
   local _RC 
   
   _IFNAME="$1"
   _IFNAME_ORIG="$2"

   _IF_IS_DOWN=0

   # is the interface up?
   if [ -n "$($IP link show $_IFNAME_ORIG | /bin/grep "state UP")" ]; then 
      _IF_IS_DOWN=1
   fi

   # bring the interface down, if we must
   if [ $_IF_IS_DOWN -ne 0 ]; then 
      $IP link set $_IFNAME_ORIG down 
      _RC=$?

      if [ $_RC -ne 0 ]; then 
         
         return $_RC 
      fi
   fi

   $IP link set $_IFNAME_ORIG name $_IFNAME 
   _RC=$?

   if [ $_RC -ne 0 ]; then 
      
      # try to recover: bring it back up, if it was up originally 
      if [ $_IF_IS_DOWN -ne 0 ]; then 
         $IP link set $_IFNAME_ORIG up
      fi
      
      return $_RC
   fi
   
   # bring the link back up, if it was up originally 
   if [ $_IF_IS_DOWN -ne 0 ]; then 
      
      $IP link set $_IFNAME up
      _RC=$?
      
      if [ $_RC -ne 0 ]; then 
      
         # try to recover 
         $IP link set $_IFNAME name $_IFNAME_ORIG
         $IP link set $_IFNAME_ORIG up

         return $_RC
      fi
   fi

   return 0
}


# which interface has the given MAC?
# $1    MAC address
# print out the name of the interface
if_mac() {
   
   local _MAC 
   local _IFNAME 
   
   _MAC="$1"

   $IP link | /bin/grep -B 1 -i "$_MAC" | \
   while read _IGNORED1 _IFNAME _IGNORED2; do
      
      echo $_IFNAME | /bin/sed -r 's/://g'
      break
   done

   return 
}


# which interface has the given sysfs device path?
# $1    device path (starts with /devices)
# print out the name of the interface, if found 
if_devpath() {
   
   local _DEVPATH 
   local _IF_DEVPATH
   local _SYSFS_IFNAME
   
   _DEVPATH="$1"
   
   # TODO: while-loop with ls here-document
   for _SYSFS_IFNAME in $(/bin/ls "$VDEV_OS_SYSFS_MOUNTPOINT/class/net/"); do 
      
      _IF_DEVPATH=$(/bin/readlink "$VDEV_OS_SYSFS_MOUNTPOINT/class/net/$_SYSFS_IFNAME" | /bin/sed -r 's/^(..\/)+//g')
      _IF_DEVPATH="/$_IF_DEVPATH"
      
      if [ "$_IF_DEVPATH" = "$_DEVPATH/net/$_SYSFS_IFNAME" ]; then 
         
         # found!
         echo "$_SYSFS_IFNAME"
         break
      fi
   done

   return
}


LINECNT=0

# process ifnames
while read IFNAME ID_TYPE ID_ARG; do
   
   IFNAME_ORIG=
   
   LINECNT=$((LINECNT+1))
   
   # skip comments 
   if [ -n "$(echo $IFNAME | /bin/egrep "^#")" ]; then 
      continue 
   fi 

   # skip blank lines 
   if [ -z "$IFNAME" ]; then 
      continue 
   fi

   # skip invalid 
   if [ -z "$IFNAME" -o -z "$ID_TYPE" -o -z "$ID_ARG" ]; then 
      vdev_warn "Failed to parse line $LINECNT of $VDEV_IFNAMES_PATH"
      continue
   fi
   
   # match identifier type
   case "$ID_TYPE" in 
      
      mac)

         # find the interface with this MAC address
         if_mac "$ID_ARG"
         IFNAME_ORIG=$(if_mac "$ID_ARG")

         ;;

      devpath)

         # find the interface with this sysfs device path
         IFNAME_ORIG=$(if_devpath "$ID_ARG")
         
         ;;

      *)
         
         # unsupported 
         vdev_error "Unsupported interface identifier '$ID_TYPE'"
         
         continue
         ;;
   esac


   if [ -z "$IFNAME_ORIG" ]; then 
      
      # couldn't match ID_ARG to an existing interface
      continue
   fi

   if [ "$IFNAME_ORIG" != "$VDEV_OS_INTERFACE" ]; then
      
      # the existing interface is not the one we're interested in
      continue
   fi

   # do the rename 
   rename_if $IFNAME $IFNAME_ORIG
   RC=$?
   
   if [ $RC -ne 0 ]; then 
      
      # failed to rename 
      vdev_warn "Failed to rename '$IFNAME_ORIG' to '$IFNAME'"
      continue 
   fi
   
done < $VDEV_IFNAMES_PATH

exit 0
