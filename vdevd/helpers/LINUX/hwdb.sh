#!/bin/dash

# Helper to query the hardware database (if it exists), and extract useful properties from it.
# NOTE: does not use vdev's subr.sh--this program can run independently.

set +u

MAX_DIR_LEN=2
LOOKUP_PREFIX=
DEVPATH=
HWDB_DIR=
MOUNTPOINT="/dev"
MODALIAS=
SUBSYSTEM="/sys"
SYSFS_MOUNTPOINT=
VERBOSE=
HWDB_FS=
USAGE="Usage: $0 [-h hwdb directory] [-D devpath] [-P prefix] [-d /dev mountpoint] [-S subsystem] [-s sysfs mountpoint] [-m path to hwdb.squashfs file to mount] [-v] [MODALIAS]"

while getopts "h:D:P:d:S:s:m:v" OPT; do

   case "$OPT" in
      
      h)
         HWDB_DIR="$OPTARG"
         ;;

      D)
         DEVPATH="$OPTARG"
         ;;

      d)
         MOUNTPOINT="$OPTARG"
         ;;

      P)
         PREFIX="$OPTARG"
         ;;

      S)
         SUBSYSTEM="$OPTARG"
         ;;

      s)
         SYSFS_MOUNTPOINT="$OPTARG"
         ;;

      v)
         VERBOSE=1
         ;;

      m)
         HWDB_FS="$OPTARG"
         ;;

      *)
         
         echo >&2 "$USAGE"
         exit 1
         ;;
   esac
done

shift $((OPTIND - 1))
MODALIAS="$1"

if [ -z "$HWDB_DIR" ]; then 

   # defaults to /dev/metadata/hwdb
   HWDB_DIR="$MOUNTPOINT/metadata/hwdb"
fi

if ! [ -d "$HWDB_DIR" ]; then 

   # no hardware database available
   echo >&2 "No hardware database at $HWDB_DIR"
   exit 1
fi

# request to mount hwdb file?
if [ -n "$HWDB_FS" ] && [ -f "$HWDB_FS" ]; then 

   /bin/mount -t squashfs -o loop "$HWDB_FS" "$HWDB_DIR" 2>/dev/null
   RC=$?

   if [ $RC -ne 0 ]; then 
      
      echo >&2 "Mounting squashfs image on $HWDB_DIR failed.  Check dmesg."
      exit 1
   fi

   exit 0
fi

if [ -z "$MODALIAS" ]; then 
   
   # need a device path 
   if [ -z "$DEVPATH" ]; then 
      echo >&2 "No modalias requires -d argument"
      exit 2
   fi

   # can compose one if this is a USB device 
   if [ "$SUBSYSTEM" = "usb" ] || [ "$SUBSYSTEM" = "usb_device" ]; then 
      
      IDVENDOR="$(/bin/cat "$SYSFS_MOUNTPOINT/$DEVPATH/idVendor")"
      IDPRODUCT="$(/bin/cat "$SYSFS_MOUNTPOINT/$DEVPATH/idProduct")"

      if [ -z "$IDVENDOR" ] || [ -z "$IDPRODUCT" ]; then 
         echo >&2 "No idVendor or idProduct in $SYSFS_MOUNTPOINT/$DEVPATH"
         exit 3
      fi 

      # ensure 4 characters long...
      while [ ${#IDVENDOR} -lt 4 ]; do IDVENDOR="0${IDVENDOR}"; done
      while [ ${#IDPRODUCT} -lt 4 ]; do IDPRODUCT="0${IDPRODUCT}"; done
      
      MODALIAS="usb:v${IDVENDOR}p${IDPRODUCT}"

   else

      # no modalias 
      echo >&2 "Unable to compose MODALIAS"
      exit 3
   fi
fi

# search prefix 
PREFIX=""
if [ -n "$LOOKUP_PREFIX" ]; then 
   PREFIX="$LOOKUP_PREFIX:$MODALIAS"
else
   PREFIX="$MODALIAS"
fi

# Walk the hardware database from the modalias.
# At each directory in the hardware database, match
# the longest prefix, and chomp it off the prefix.
# Directory names are interpreted as regexes.

# break the modalias up by :, and then split into groups no longer than $MAX_DIR_LEN.
# to do so, insert : every $MAX_DIR_LEN characters 
EXPR="s/\([^:]\{${MAX_DIR_LEN}\}\)/\1:/g"
PREFIX="$(echo "$PREFIX" | /bin/sed $EXPR)"

OLDIFS="$IFS"
IFS=":"
set -- $PREFIX
IFS="$OLDIFS"

HWDB_PATH="$HWDB_DIR"
ALL_MATCH=0
BEST_REGEX_MATCH=""

while [ $# -gt 0 ]; do

   # stop looking if we're at the leaf, and the last regex ended in *
   if [ $ALL_MATCH -eq 1 ] && [ -f "$HWDB_PATH/properties" ]; then 
      break
   fi
   
   BEST_REGEX_MATCH=""
   BEST_REGEX_MATCH_LEN=0

   PREFIX="$1"
   shift 1

   # find the longest (i.e. most specific) prefix match 
   # NOTE: assumes no spaces (which works for modaliases)
   while read -r HWDB_REGEX; do 
      
      MATCH_REGEX="$HWDB_REGEX"
      
      # append a * to match the trailing modalias, if we need to 
      MATCH_REGEX_NOSTAR="${MATCH_REGEX%\*}"
      if [ ${#MATCH_REGEX_NOSTAR} -eq ${#MATCH_REGEX} ]; then
      
         # exact match
         ALL_MATCH=0
         MATCH_REGEX="${MATCH_REGEX}*"
      else 
         
         # MATCH_REGEX ends in *
         ALL_MATCH=1
      fi
      
      case $PREFIX in
         
         $MATCH_REGEX)
            
            # match! remember the longest, but prefer exact over any
            if [ $ALL_MATCH -eq 1 ] && [ ${#HWDB_REGEX} -gt $BEST_REGEX_MATCH_LEN ]; then 
               
               BEST_REGEX_MATCH="$HWDB_REGEX"
               BEST_REGEX_MATCH_LEN=${#BEST_REGEX_MATCH}

            elif [ $ALL_MATCH -eq 0 ] && [ ${#HWDB_REGEX} -ge $BEST_REGEX_MATCH_LEN ]; then 

               
               BEST_REGEX_MATCH="$HWDB_REGEX"
               BEST_REGEX_MATCH_LEN=${#BEST_REGEX_MATCH}
            fi

            # if there are properties here, then enumerate them
            if [ $ALL_MATCH -eq 1 ] && [ -f "$HWDB_PATH/$HWDB_REGEX/properties" ]; then
               
               /bin/cat "$HWDB_PATH/$HWDB_REGEX/properties"
            fi
            ;;
      esac
   done << EOF
$(/bin/ls "$HWDB_PATH")
EOF

   if [ -z "$BEST_REGEX_MATCH" ]; then 
      # no more matches 
      break
   fi

   HWDB_PATH="$HWDB_PATH/$BEST_REGEX_MATCH"
done

exit 0
