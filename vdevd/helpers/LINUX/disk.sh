#!/bin/dash

# vdev helper for setting up symlinks to disks and partitions 
# works for ATA, SATA, SCSI, CCIS, MMC, Memstick, and USB disks, and their partitions.
# TODO: refactor into disk-$TYPE.sh helpers, and have disk.sh source them and invoke them as needed.

. "$VDEV_HELPERS/subr.sh"

# entry point
# return 0 on success
# return 2 on disk helper failure
# return 3 on unknown disk type
# return 5 on unknown partition
# return 10 on unknown action
main() {

   local DISK_TYPE DISK_ID DISK_SERIAL DISK_WWN DISK_NAME PART_NAME 
   local VDEV_BUS VDEV_ATA VDEV_SCSI VDEV_CCIS VDEV_USB VDEV_IEEE1394 VDEV_LOOP VDEV_VIRTIO VDEV_MMC VDEV_SERIAL VDEV_USB_SERIAL VDEV_SERIAL_SHORT VDEV_REVISION
   local VDEV_PROPERTIES
   local HELPER STAT_RET HELPER_DATA 
   
   # blkid
   local UUID LABEL PARTUUID PARTLABEL PTUUID PTTYPE TYPE USAGE WWN BLKID_DATA
   local PART_ENTRY_SCHEME PART_ENTRY_UUID PART_ENTRY_TUPE PART_ENTRY_NUMBER PART_ENTRY_OFFSET PART_ENTRY_SIZE PART_ENTRY_DISK
   
   # properties 
   local VDEV_PART_ENTRY_TYPE VDEV_PART_TABLE_TYPE VDEV_PART_ENTRY_SCHEME VDEV_PART_ENTRY_UUID VDEV_PART_ENTRY_DISK VDEV_PART_ENTRY_NUMBER VDEV_PART_ENTRY_OFFSET VDEV_PART_ENTRY_SIZE
   local VDEV_FS_TYPE VDEV_FS_USAGE VDEV_TYPE VDEV_WWN ALL_PROPS VDEV_ATA_WWN

   # parent device query
   local PROP_KEY PROP_VALUE PROP_KEY_AND_VALUE
   
   # partition data 
   local PARENT_DEVICE_PATH SERIALIZED_PARENT_DEVICE_PATH PARENT_METADATA_DIR PARENT_PROPERTIES EXISTING

   # removable 
   local REMOVABLE_FILE

   # pvs 
   local PVS_DATA
   
   # if we're removing this disk, just blow away its symlinks
   if [ "$VDEV_ACTION" = "remove" ]; then 

      vdev_cleanup "$VDEV_METADATA"
      return 0
   fi

   # otherwise, we're adding links.  Make sure...
   if [ "$VDEV_ACTION" != "add" ]; then 

      vdev_error "Unknown action '$VDEV_ACTION'"
      return 10
   fi

   # skip inappropriate block devices 
   if [ -n "$(echo "$VDEV_PATH" | /bin/egrep "fd*|mtd*|nbd*|gnbd*|btibm*|dm-*|md*|zram*|mmcblk[0-9]*rpmb")" ]; then 
      return 0
   fi

   SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"

   # determine disk type, serial, and other tidbits
   DISK_TYPE=""
   DISK_ID=""
   DISK_SERIAL=""
   DISK_WWN=""
   DISK_NAME=""
   PART_NAME=""

   VDEV_TYPE=""
   VDEV_BUS=""
   VDEV_ATA=""
   VDEV_SCSI=""
   VDEV_CCIS=""
   VDEV_USB=""
   VDEV_MMC=""
   VDEV_IEEE1394=""
   VDEV_LOOP=""
   VDEV_VIRTIO=""
   
   VDEV_SERIAL=""
   VDEV_SERIAL_SHORT=""
   VDEV_REVISION=""

   VDEV_PROPERTIES=""

   if [ "$VDEV_OS_DEVTYPE" != "partition" ]; then 
      
      # this is a disk, not a partition 

      if [ -n "$(echo "$VDEV_PATH" | /bin/egrep "vd.*[0-9]$")" ]; then 
         
         # virtio block device 
         DISK_TYPE="virtio"

      elif [ -n "$(echo "$VDEV_PATH" | /bin/egrep "loop[0-9]+")" ]; then 

         # loopback disk 
         DISK_TYPE="loop"

      elif [ -n "$(echo "$VDEV_PATH" | /bin/egrep "sd[^0-9]+$|sr.*")" ]; then 

         # scsi, (s)ata, ieee1394, or USB disk
         if [ -n "$(vdev_subsystems "$SYSFS_PATH" | /bin/grep "scsi")" ]; then 

            # part of a scsi device
            VENDOR_FILE="$SYSFS_PATH/device/vendor"
            TYPE_FILE="$SYSFS_PATH/device/type"
            SCSI_LEVEL_FILE="$SYSFS_PATH/device/scsi_level"
            
            if [ -f "$VENDOR_FILE" ] && [ -n "$(/bin/cat "$VENDOR_FILE" | /bin/grep "ATA")" ]; then 

               # ATA disk using the scsi subsystem 
               DISK_TYPE="ata"
            
            elif [ -f "$TYPE_FILE" ] && [ -f "$SCSI_LEVEL_FILE" ] && 
                 [ -n "$(/bin/fgrep "5" "$TYPE_FILE")" ] && \
                 [ -n "$(/bin/egrep "[6-9]+" "$SCSI_LEVEL_FILE")" ]; then 

               # ATA/ATAPI devices (SPC-3 or later) using the scsi subsystem 
               DISK_TYPE="ata"
            fi
         fi
         
         # maybe a usb device instead?
         if [ -z "$DISK_TYPE" ] && [ -n "$(vdev_subsystems "$SYSFS_PATH" | /bin/grep "usb")" ]; then 

            # part of a USB device
            REMOVABLE_FILE="$SYSFS_PATH/removable"
            
            if ! [ -f "$REMOVABLE_FILE" ] || [ -n "$(/bin/fgrep "0" "$REMOVABLE_FILE")" ]; then 

               # non-removable USB mass storage in a (S)ATA enclosure
               # see if we can probe with stat_ata; otherwise fall back to stat_usb
               HELPER_DATA="$($VDEV_HELPERS/stat_ata "$VDEV_MOUNTPOINT/$VDEV_PATH")"
               STAT_RET=$?

               if [ $STAT_RET -eq 0 ]; then

                   # can treat as ATA disk
                   DISK_TYPE="ata"
               else

                   # fall back to USB 
                   DISK_TYPE="usb"
               fi
            
            else

               # usb mass-removable storage disk 
               DISK_TYPE="usb"

            fi
         fi
         
         # maybe a firewire device instead?
         if [ -z "$DISK_TYPE" ]; then  

            DISK_ID="$(vdev_getattrs "ieee1394_id" "$SYSFS_PATH")"

            if [ -n "$DISK_ID" ]; then 

               # take the first one 
               OLDIFS="$IFS"
               set -- $DISK_ID
               IFS="$OLDIFS"
               
               DISK_ID="$1"
               DISK_SERIAL="$1"
               DISK_TYPE="ieee1394"
            fi
         fi

         if [ -z "$DISK_TYPE" ]; then 

            # generic scsi? Try to probe for it later.
            DISK_TYPE="scsi"
         fi

      elif [ -n "$(echo "$VDEV_PATH" | /bin/egrep "cciss.*")" ] && [ "$VDEV_OS_DEVTYPE" = "disk" ]; then 

         # CCISS (HP smart array)
         DISK_TYPE="cciss"

      elif [ -n "$(echo "$VDEV_PATH" | /bin/egrep "mmcblk[0-9]+|mmcblk[0-9]+p[0-9]+")" ]; then 

         if [ -n "$(vdev_subsystems "$SYSFS_PATH" | /bin/grep "mmc")" ]; then 

            # mmcblk 
            DISK_TYPE="mmc"

         elif [ -n "$(vdev_subsystems "$SYSFS_PATH" | /bin/grep "memstick")" ]; then 

            # memstick 
            DISK_TYPE="memstick"
         fi
      fi

      # is this a disk type this script supports?
      if [ -z "$DISK_TYPE" ]; then 
         vdev_error "Could not detect disk type for $VDEV_OS_DEVPATH"
         return 3
      fi

   elif ! [ -e "$SYSFS_PATH/whole_disk" ]; then 
      
      # partition.
      # find the parent device.
      PARENT_DEVICE_PATH="$(vdev_device_parent "$SYSFS_PATH")"
      RC=$?

      if [ $RC -ne 0 ]; then 

         # no parent 
         vdev_error "unrecognizable partition: no parent device found for $VDEV_OS_DEVPATH"
         return 5
      fi

      SERIALIZED_PARENT_DEVICE_PATH="$(vdev_serialize_path "$PARENT_DEVICE_PATH")"
      
      # get the relevant info from the parent device 
      PARENT_METADATA_DIR="$(/bin/readlink "$VDEV_GLOBAL_METADATA/sysfs/$SERIALIZED_PARENT_DEVICE_PATH")"
      RC=$?
      
      if [ $RC -ne 0 ] || [ -z "$PARENT_METADATA_DIR" ] || ! [ -f "$PARENT_METADATA_DIR/properties" ]; then 

         # could not find parent metadata...maybe we didn't process it yet?  but this would be a bug, since we process devices in 
         # breadth-first order, from parents to children
         vdev_error "partition not found: no metadata on parent device of $VDEV_OS_DEVPATH (at $VDEV_GLOBAL_METADATA/sysfs/$SERIALIZED_PARENT_DEVICE_PATH)"
         return 5
      fi

      PARENT_PROPERTIES="$PARENT_METADATA_DIR/properties"
      
      # make sure we can query non-existent variables...
      set +u

      PROP_KEY=
      PROP_VALUE=
      PROP_KEY_AND_VALUE=
      
      # load all parent metadata, but don't override stuff vdevd gave us
      while read -r PROP_KEY_AND_VALUE; do 

         OLDIFS="$IFS"
         IFS="="
         set -- $PROP_KEY_AND_VALUE
         IFS="$OLDIFS"

         if [ $# -lt 2 ]; then 
            continue
         fi

         PROP_KEY="$1"
         PROP_VALUE="$2"
         
         # join remaining parts 
         shift 2
         while [ $# -gt 0 ]; do 
            PROP_VALUE="$PROP_VALUE=$1"
            shift 1
         done
         
         EXISTING=
         eval "EXISTING=\$$PROP_KEY"
         if [ -n "$EXISTING" ]; then 
            continue 
         fi
         
         eval "$PROP_KEY=\"$PROP_VALUE\""
      done < "$PARENT_PROPERTIES"

      set -u

      DISK_TYPE="$VDEV_TYPE"

      if [ -z "$DISK_TYPE" ]; then 

         # failed to load parent info
         vdev_error "partition device $VDEV_OS_DEVPATH not identified by parent ($PARENT_DEVICE_PATH)"
         return 5
      fi
      
   else 
      
      # we don't support whole-disk partitions 
      vdev_error "Nothing to do for whole-disk partition $VDEV_OS_DEVPATH"
      return 0
   fi

   HELPER=
   HELPER_DATA=
   STAT_RET=0
   VDEV_ATA_WWN=""

   # query information about the disk
   case "$DISK_TYPE" in

      ata)

         HELPER="stat_ata"

         # (S)ATA disk--probe if we haven't already
         if [ -z "$HELPER_DATA" ]; then 

            HELPER_DATA="$($VDEV_HELPERS/stat_ata "$VDEV_MOUNTPOINT/$VDEV_PATH")"
            STAT_RET=$?
         fi

         if [ $STAT_RET -eq 0 ]; then 

            # success!
            eval "$HELPER_DATA"

            # disk id is the serial number
            DISK_ID="$VDEV_ATA_SERIAL"
            DISK_SERIAL="$VDEV_ATA_SERIAL"

            # disk wwn is given by stat_ata 
            DISK_WWN="$VDEV_ATA_WWN"
            
            # vdev properties 
            VDEV_ATA="1"
            VDEV_BUS="ata"
            VDEV_TYPE="$VDEV_ATA_TYPE"
            VDEV_SERIAL="$VDEV_ATA_SERIAL"
            VDEV_SERIAL_SHORT="$VDEV_ATA_SERIAL_SHORT"
            VDEV_REVISION="$VDEV_ATA_REVISION"
         fi 
         
         ;;

      scsi)

         HELPER="stat_scsi"
         
         # generic SCSI disk 
         HELPER_DATA="$($VDEV_HELPERS/stat_scsi -d "$VDEV_MOUNTPOINT/$VDEV_PATH")"
         STAT_RET=$?

         if [ $STAT_RET -eq 0 ]; then 

            # success!
            if [ "$VDEV_SCSI" != "1" ]; then 
               
               # not a scsi disk
               STAT_RET=255
               
            else

               # generic SCSI disk 
               # disk ID is the serial number
               DISK_ID="$VDEV_SCSI_SERIAL"
               DISK_SERIAL="$VDEV_SCSI_SERIAL"
               
               # vdev properties       
               VDEV_SCSI="1"
               VDEV_BUS="scsi"
               VDEV_SCSI="$VDEV_SCSI_SERIAL"
            fi
         fi
         ;;

      cciss)

         HELPER="stat_scsi"

         # HP smart raid 
         HELPER_DATA="$($VDEV_HELPERS/stat_scsi -d "$VDEV_MOUNTPOINT/$VDEV_PATH")"
         STAT_RET=$?
         
         if [ $STAT_RET -eq 0 ]; then 

            # disk ID is the serial number
            DISK_ID="$VDEV_SCSI_SERIAL"
            DISK_SERIAL="$VDEV_SCSI_SERIAL"
            
            # vdev properties 
            VDEV_CCISS="1"
            VDEV_BUS="scsi"
            VDEV_SERIAL="$VDEV_SCSI_SERIAL"
         fi
         
         ;;

      usb)
         
         HELPER="stat_usb"
         
         # USB disk
         VDEV_USB_SERIAL=""
         HELPER_DATA="$($VDEV_HELPERS/stat_usb "$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH")"
         STAT_RET=$?

         if [ $STAT_RET -eq 0 ]; then 

            # disk id is the serial number
            DISK_ID="$VDEV_USB_SERIAL"
            DISK_SERIAL="$VDEV_USB_SERIAL"

            # vdev properties 
            VDEV_USB="1"
            VDEV_BUS="usb"
            VDEV_SERIAL="$VDEV_USB_SERIAL"
         fi
         
         ;;

      ieee1394) 

         # ieee1394 device 
         # DISK_SERIAL already set above
         VDEV_BUS="ieee1934"
         VDEV_SERIAL="$DISK_SERIAL"
         VDEV_IEEE1394="1"
         
         ;;

      mmc|memstick)
         
         HELPER="vdev_getattr"
         
         # mmc device 
         MMC_NAME="$(vdev_getattr "name" "$SYSFS_PATH")"
         DISK_SERIAL="$(vdev_getattr "serial" "$SYSFS_PATH")"
         
         if [ -n "$MMC_NAME" ] && [ -n "$DISK_SERIAL" ]; then 
            
            DISK_ID="${MMC_NAME}_${DISK_SERIAL}"
         else

            # not found
            STAT_RET=1
         fi
         
         VDEV_SERIAL="$DISK_SERIAL"
         VDEV_MMC="1"
         
         ;;

      loop)
      
         VDEV_LOOP="1"
         ;;

      virtio)

         VDEV_VIRTIO="1"
         ;;
         
      *)
         
         # unknown type
         vdev_error "Unknown disk type '$DISK_TYPE'"
         return 3
         ;;

   esac

   # verify stat'ing succeeded; otherwise we're done (can't label by id)
   if [ $STAT_RET -ne 0 ]; then 
      vdev_error "$HELPER exited with $STAT_RET"
      return 2
   fi

   # success! import the extra metadata
   eval "$HELPER_DATA"

   # put label into place
   DISK_NAME="$DISK_TYPE-$DISK_ID"

   # if this is a partition, then get the partition number too 
   if [ "$VDEV_OS_DEVTYPE" = "partition" ]; then
      
      # this is a partition, but not a whole-disk partition 
      SYSFS_PART_FILE="$SYSFS_PATH/partition"

      if ! [ -f "$SYSFS_PART_FILE" ]; then 

         vdev_error "unknown partition: no such file or directory $SYSFS_PART_FILE"
         return 5
      fi
      
      PART="$(/bin/cat "$SYSFS_PART_FILE")"
      
      if [ -z "$PART" ]; then 
         vdev_error "unknown partition"
         return 5
      fi
      
      # include partition ID in the disk name
      PART_NAME="part${PART}"
      DISK_NAME="$DISK_NAME-$PART_NAME"
   fi

   # get disk UUID, LABEL, PARTUUID, partition table UUID, partition table type, FS type
   UUID=""
   LABEL=""
   PARTUUID=""
   PARTLABEL=""
   PTUUID=""
   PTTYPE=""
   TYPE=""
   USAGE=""
   WWN=""
   PART_ENTRY_SCHEME=""
   PART_ENTRY_UUID=""
   PART_ENTRY_TYPE=""
   PART_ENTRY_NUMBER=""
   PART_ENTRY_OFFSET=""
   PART_ENTRY_SIZE=""
   PART_ENTRY_DISK=""
   BLKID_DATA=""
   
   if [ -x /sbin/blkid ]; then 
      if [ -z "$(echo "$VDEV_PATH" | /bin/egrep "sr.*")" ]; then 
         
         BLKID_DATA="$(vdev_blkid "$VDEV_MOUNTPOINT/$VDEV_PATH")"
         STAT_RET=$?

         if [ $STAT_RET -ne 0 ]; then

            # exit 2 means that we didn't get any information (this is okay by us)
            vdev_warn "/sbin/blkid failed on $VDEV_MOUNTPOINT/$VDEV_PATH, exit status $STAT_RET"
         else

            # make sure everything is properly escaped before evaluating
            BLKID_DATA="$(echo "$BLKID_DATA" | /bin/sed "s/^\([^=]\+\)=\(.*\)$/\1='\2'/g")"
            eval "$BLKID_DATA"
         fi
      fi
   else
      vdev_warn "Could not find blkid in /sbin/blkid.  $VDEV_MOUNTPOINT/disk/by-*/ symlinks will not be added."
   fi

   # get disk WWN, if set 
   if [ -n "$DISK_WWN" ]; then
      WWN="wwn-$DISK_WWN"

      if [ -n "$PART_NAME" ]; then
         WWN="$WWN-$PART_NAME"
      fi
   fi

   
   # add the disk links
   if [ -n "$DISK_NAME" ] && [ -n "$DISK_ID" ]; then
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/$DISK_NAME" "$VDEV_METADATA"
   fi

   if [ -n "$UUID" ]; then
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-uuid/$UUID" "$VDEV_METADATA"
   fi

   if [ -n "$LABEL" ]; then 
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-label/$LABEL" "$VDEV_METADATA"
   fi

   if [ -n "$WWN" ]; then
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/$WWN" "$VDEV_METADATA"
   fi

   if [ -n "$PARTUUID" ]; then 
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-partuuid/$PARTUUID" "$VDEV_METADATA"
   fi

   if [ -n "$PARTLABEL" ]; then 
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-partlabel/$PARTLABEL" "$VDEV_METADATA"
   fi

   # is this a physical volume?
   PVS_RC=1
   PVS_DATA=""
   LVM2_PV_UUID=""

   # TODO: don't scan the whole /dev
   if [ -x /sbin/pvs ]; then 
      PVS="/sbin/pvs --nameprefixes --noheadings $VDEV_MOUNTPOINT/$VDEV_PATH"
      PVS_DATA="$($PVS -o pv_uuid 2>"$VDEV_MOUNTPOINT/null")"
      PVS_RC=$?
   else
      vdev_warn "Could not find pvs in /sbin/pvs.  LVM physical volume symlinks in $VDEV_MOUNTPOINT/disk/by-id will not be created."
   fi

   if [ $PVS_RC -eq 0 ] && [ -n "$PVS_DATA" ]; then 
      
      eval "$PVS_DATA"

      if [ -n "$LVM2_PV_UUID" ]; then 
      
         # this is a PV
         vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/lvm-pv-uuid-$LVM2_PV_UUID" "$VDEV_METADATA"
      fi
   fi

   # set ownership and bits 
   vdev_permissions $VDEV_VAR_DISK_OWNER:$VDEV_VAR_DISK_GROUP $VDEV_VAR_DISK_MODE "$VDEV_MOUNTPOINT/$VDEV_PATH"

   # device properties 
   VDEV_PART_ENTRY_TYPE="$PART_ENTRY_TYPE"
   VDEV_PART_TABLE_TYPE="$PART_ENTRY_SCHEME"
   VDEV_PART_ENTRY_SCHEME="$PART_ENTRY_SCHEME"
   VDEV_PART_ENTRY_UUID="$PART_ENTRY_UUID"
   VDEV_PART_ENTRY_DISK="$PART_ENTRY_DISK"
   VDEV_PART_ENTRY_NUMBER="$PART_ENTRY_NUMBER"
   VDEV_PART_ENTRY_OFFSET="$PART_ENTRY_OFFSET"
   VDEV_PART_ENTRY_SIZE="$PART_ENTRY_SIZE"
   VDEV_FS_TYPE="$TYPE"
   VDEV_FS_USAGE="$USAGE"
   VDEV_TYPE="$DISK_TYPE"
   VDEV_WWN=""

   ALL_PROPS="VDEV_BUS VDEV_SERIAL VDEV_SERIAL_SHORT VDEV_REVISION VDEV_PART_ENTRY_DISK VDEV_PART_ENTRY_NUMBER VDEV_PART_ENTRY_OFFSET VDEV_PART_ENTRY_SIZE VDEV_PART_ENTRY_SCHEME VDEV_PART_ENTRY_TYPE VDEV_PART_TABLE_TYPE VDEV_PART_ENTRY_UUID VDEV_FS_TYPE VDEV_FS_USAGE VDEV_TYPE VDEV_MAJOR VDEV_MINOR VDEV_OS_SUBSYSTEM VDEV_OS_DEVTYPE VDEV_OS_DEVPATH VDEV_OS_DEVNAME $VDEV_PROPERTIES"

   # identify type as a type-specific flag
   case "$DISK_TYPE" in

      ata)
         
         VDEV_WWN="$VDEV_ATA_WWN"
         ALL_PROPS="VDEV_ATA VDEV_WWN $ALL_PROPS"
         ;;

      usb)

         ALL_PROPS="VDEV_USB $ALL_PROPS"      
         ;;

      scsi)

         ALL_PROPS="VDEV_SCSI $ALL_PROPS"
         ;;

      cciss)

         ALL_PROPS="VDEV_CCISS $ALL_PROPS"
         ;;

      ieee1394)

         ALL_PROPS="VDEV_IEEE1394 $ALL_PROPS"
         ;;
         
      mmc|memstick)

         ALL_PROPS="VDEV_MMC $ALL_PROPS"
         ;;

      loop)

         ALL_PROPS="VDEV_LOOP $ALL_PROPS"
         ;;

      virtio)

         ALL_PROPS="VDEV_VIRTIO $ALL_PROPS"
         ;;

   esac

   # set all device properties
   vdev_add_properties "$VDEV_METADATA" $ALL_PROPS

   return 0
}

if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
