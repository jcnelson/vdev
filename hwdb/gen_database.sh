#!/bin/dash

# Compile the contents of a set of hwdb files into a directory hierarchy.
# Depends on:
# - dash
# - coreutils
# - findutils

# determine whether or not a string's first non-whitespace character is a '#'
# return 0 if so 
# return 1 if not
is_comment() {
   
   local _LEN1 _LEN2 _STRIPPED
   
   set -- $1

   _LEN1=${#1}
   _STRIPPED=${1###}
   _LEN2=${#_STRIPPED}

   if [ $_LEN1 -eq $_LEN2 ]; then 
      # not a comment--it doesn't start with #
      return 1
   else 
      return 0
   fi
}

# determine whether or not a string is a modalias regex.
# modalias regexes are *not* indented, which is how we check
# return 0 if so 
# return 1 if not 
is_modalias_regex() {

   local _LEN1 _LEN2 _STRIPPED

   _LEN1=${#1}
   _STRIPPED=${1## }
   _LEN2=${#_STRIPPED}

   if [ $_LEN1 -eq $_LEN2 ]; then 
      # not indented--this is a modalias regex 
      return 0
   else 
      return 1
   fi
}


# extract a hardware database file to a given directory, read from stdin.
# Construct a trie on disk, using directories as non-leaf nodes to identify 
# substrings within the modalias regex.
# $1    The output directory 
# Return 0 on success
# return nonzero on failure
extract_hwdb() {

   local _MODALIAS_REGEX _PROP _LINE _OUT_DIR _TRIE_PATH _READING_PROPS _ALL_PROPS _OLDIFS _PROP_NAME

   _OUT_DIR="$1"

   if [ -z "$_OUT_DIR" ]; then 
      return 1
   fi

   _MODALIAS_REGEX=
   _PROP=
   _READING_PROPS=
   _ALL_PROPS=
   _OLDIFS="$IFS"

   while read _LINE; do
      
      if [ -z "$_LINE" ]; then 
         # no longer reading properties--empty line 
         # indicates end of property block
         # record what properties we discovered, and look for the next line
         _READING_PROPS=

         if [ -n "$_ALL_PROPS" ]; then 
            echo "VDEV_HWDB_PROPERTIES=\"$_ALL_PROPS\"" >> "$_OUT_DIR/$_TRIE_PATH/properties"
            _ALL_PROPS=
         fi
         
         continue 
      fi

      # skip comments
      if is_comment "$_LINE"; then 
         continue
      fi

      if ! [ $_READING_PROPS ]; then 

         # modalias regex? as in, *not* indented?
         if is_modalias_regex "$_LINE"; then 
         
            # replace : with / to "unprefix" it
            _TRIE_PATH="$(echo "$_LINE" | /bin/sed -r 's/:/\//g')"
            
            /bin/mkdir -p "$_OUT_DIR/$_TRIE_PATH"
            
            # will begin reading properties
            _READING_PROPS=1
            
            if [ -f "$_OUT_DIR/$_TRIE_PATH/properties" ]; then 
               /bin/rm -f "$_OUT_DIR/$_TRIE_PATH/properties"
            fi
         fi
      
      else
         
         # write properties, but prefix them so we can find them later
         if [ -n "$_LINE" ]; then 
            echo "VDEV_HWDB_${_LINE}" >> "$_OUT_DIR/$_TRIE_PATH/properties"

            # extract property name...
            IFS="="
            
            set -- $_LINE
            _PROP_NAME="$1"
            
            IFS=$_OLDIFS

            _ALL_PROPS="VDEV_HWDB_${_PROP_NAME} $_ALL_PROPS"
         fi
      fi

   done

   return 0
}


# print usage 
usage() {
   
   echo >&2 "Usage: $1 -o OUTPUT_DIR [-v] [.hwdb file [.hwdb file...]]"
}



# execution starts here...
PROGNAME="$0"
UDEV_HWDB_DIR=
OUTPUT_DIR=
VERBOSE=
RC=0

while getopts "o:v" OPT; do

   case "$OPT" in

      o)
         OUTPUT_DIR="$OPTARG"
         ;;

      v)
         VERBOSE=1
         ;;

      *)
         usage "$PROGNAME"
         exit 1
         ;;
   esac
done

shift $((OPTIND - 1))

if [ -z "$OUTPUT_DIR" ]; then 
   
   usage "$PROGNAME"
   exit 1
fi

while [ $# -gt 0 ]; do
   
   HWDB_PATH="$1"
   shift 1
   
   if [ $VERBOSE ]; then 
      echo "Extracting $HWDB_PATH"
   fi

   extract_hwdb "$OUTPUT_DIR" < "$HWDB_PATH"
   RC=$?
   
   if [ $RC -ne 0 ]; then 
      echo >&2 "Failed to extract $HWDB_PATH"
      break
   fi
done

exit $RC
