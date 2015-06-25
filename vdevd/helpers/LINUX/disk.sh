#!/bin/dash

# vdev helper for setting up symlinks to disks and partitions 
# works for ATA, SATA, and USB disks

. "$VDEV_HELPERS/subr.sh"

# if we're removing this disk, just blow away its symlinks
if [ "$VDEV_ACTION" = "remove" ]; then 

   vdev_cleanup "$VDEV_METADATA"
   exit 0
fi

# otherwise, we're adding links.  Make sure...
if [ "$VDEV_ACTION" != "add" ]; then 

   vdev_fail 10 "Unknown action '$VDEV_ACTION'"
fi

# skip inappropriate block devices 
if [ -n "$(echo "$VDEV_PATH" | /bin/egrep "fd*|mtd*|nbd*|gnbd*|btibm*|dm-*|md*|zram*|mmcblk[0-9]*rpmb")" ]; then 
   exit 0
fi

SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"

# determine disk type, serial, and other tidbits
DISK_TYPE=
DISK_ID=
DISK_SERIAL=
DISK_WWN=
DISK_NAME=
PART_NAME=

VDEV_BUS=
VDEV_ATA=
VDEV_SCSI=
VDEV_CCIS=
VDEV_USB=
VDEV_SERIAL=
VDEV_SERIAL_SHORT=
VDEV_REVISION=

VDEV_PROPERTIES=

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
            [ -n "$(/bin/cat "$TYPE_FILE" | /bin/grep "5")" ] && \
            [ -n "$(/bin/cat "$SCSI_LEVEL_FILE" | /bin/egrep "[6-9]+")" ]; then 

            # ATA/ATAPI devices (SPC-3 or later) using the scsi subsystem 
            DISK_TYPE="ata"
         
         else
            
            # generic scsi? Try to probe for it later
            DISK_TYPE="scsi"

         fi

      elif [ -n "$(vdev_subsystems "$SYSFS_PATH" | /bin/grep "usb")" ]; then 

         # part of a USB device
         REMOVABLE_FILE="$SYSFS_PATH/removable"
         
         if [ -f "$REMOVABLE_FILE" ] && [ -n "$(/bin/cat "$REMOVABLE_FILE" | /bin/grep "1")" ]; then 

            # non-removable USB mass storage in a (S)ATA enclosure 
            DISK_TYPE="ata"
         
         else

            # usb mass-removable storage disk 
            DISK_TYPE="usb"

         fi
      
      else

         # maybe a firewire device?
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
   test -z "$DISK_TYPE" && vdev_fail 0 "Could not detect disk type for $VDEV_OS_DEVPATH"

elif ! [ -e "$SYSFS_PATH/whole_disk" ]; then 

   # find the parent device 
   PARENT_DEVICE_PATH="$(vdev_device_parent "$SYSFS_PATH")"
   RC=$?

   if [ $RC -ne 0 ]; then 

      # no parent 
      vdev_fail 5 "unrecognizable partition: no parent device found for $VDEV_OS_DEVPATH"
   fi

   SERIALIZED_PARENT_DEVICE_PATH="$(vdev_serialize_path "$PARENT_DEVICE_PATH")"
   
   # get the relevant info from the parent device 
   PARENT_METADATA_DIR="$(/bin/readlink "$VDEV_GLOBAL_METADATA/sysfs/$SERIALIZED_PARENT_DEVICE_PATH")"
   RC=$?
   
   if [ $RC -ne 0 ] || [ -z "$PARENT_METADATA_DIR" ] || ! [ -f "$PARENT_METADATA_DIR/properties" ]; then 

      # could not find parent metadata...maybe we didn't process it yet?  but this would be a bug, since we process devices in 
      # breadth-first order, from parents to children
      vdev_fail 5 "partition not found: no metadata on parent device of $VDEV_OS_DEVPATH ($PARENT_DEVICE_PATH)"
   fi

   PARENT_PROPERTIES="$PARENT_METADATA_DIR/properties"
   
   # load all parent metadata, but don't override stuff vdevd gave us
   while read -r PROP_KEY_AND_VALUE; do 

      OLDIFS="$IFS"
      IFS="="
      set -- $PROP_KEY_AND_VALUE
      IFS="$OLDIFS"

      PROP_KEY="$1"

      EXISTING=
      eval "EXISTING=\$$PROP_KEY"
      if [ -n "$EXISTING" ]; then 
         continue 
      fi
      
      eval "$PROP_KEY_AND_VALUE"
   done < "$PARENT_PROPERTIES"

   DISK_TYPE="$VDEV_TYPE"

   if [ -z "$DISK_TYPE" ]; then 

      # failed to load parent info
      vdev_fail 5 "partition device $VDEV_OS_DEVPATH not identified by parent ($PARENT_DEVICE_PATH)"
   fi
   
else 
   
   # we don't support whole-disk partitions 
   vdev_fail 0 "Nothing to do for whole-disk partition $VDEV_OS_DEVPATH"
fi

HELPER=
STAT_RET=0

# query information about the disk
case "$DISK_TYPE" in

   ata)

      HELPER="stat_ata"

      # (S)ATA disk
      eval "$($VDEV_HELPERS/stat_ata "$VDEV_MOUNTPOINT/$VDEV_PATH")"
      STAT_RET=$?

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
      
      ;;

   scsi)

      HELPER="stat_scsi"
      
      # generic SCSI disk 
      eval "$($VDEV_HELPERS/stat_scsi -d "$VDEV_MOUNTPOINT/$VDEV_PATH")"
      STAT_RET=$?
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
      ;;

   cciss)

      HELPER="stat_scsi"

      # HP smart raid 
      eval "$($VDEV_HELPERS/stat_scsi -d "$VDEV_MOUNTPOINT/$VDEV_PATH")"
      STAT_RET=$?
      
      # disk ID is the serial number
      DISK_ID="$VDEV_SCSI_SERIAL"
      DISK_SERIAL="$VDEV_SCSI_SERIAL"
      
      # vdev properties 
      VDEV_CCISS="1"
      VDEV_BUS="scsi"
      VDEV_SERIAL="$VDEV_SCSI_SERIAL"
      
      ;;

   usb)
      
      HELPER="stat_usb"
      
      # USB disk
      eval "$($VDEV_HELPERS/stat_usb "$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH")"
      STAT_RET=$?

      # disk id is the serial number
      DISK_ID="$VDEV_USB_SERIAL"
      DISK_SERIAL="$VDEV_USB_SERIAL"

      # vdev properties 
      VDEV_USB="1"
      VDEV_BUS="usb"
      VDEV_SERIAL="$VDEV_USB_SERIAL"

      ;;

   ieee1394) 

      # ieee1394 device 
      # DISK_SERIAL already set above
      VDEV_BUS="ieee1934"
      VDEV_SERIAL="$DISK_SERIAL"
      
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
         _RC=1
      fi
      
      VDEV_SERIAL="$DISK_SERIAL"
      ;;

   loop)
   
      # nothing to do 
      ;;
      
   *)
      
      # unknown type
      vdev_fail 2 "Unknown disk type '$DISK_TYPE'"
      ;;

esac

# verify stat'ing succeeded; otherwise we're done (can't label by id)
test 0 -ne $STAT_RET && vdev_fail 3 "$HELPER exited with $STAT_RET"

# verify that we got a disk ID; otherwise we're done (no label to set)
# test -z "$DISK_ID" && vdev_fail 4 "unknown disk ID for $VDEV_OS_DEVPATH"

# put label into place
DISK_NAME="$DISK_TYPE-$DISK_ID"

# if this is a partition, then get the partition number too 
if [ "$VDEV_OS_DEVTYPE" = "partition" ]; then
   
   # this is a partition, but not a whole-disk partition 
   SYSFS_PART_FILE="$SYSFS_PATH/partition"

   if ! [ -f "$SYSFS_PART_FILE" ]; then 

      vdev_fail 5 "unknown partition: no such file or directory $SYSFS_PART_FILE"
   fi
   
   PART="$(/bin/cat "$SYSFS_PART_FILE")"
   
   test -z "$PART" && vdev_fail 5 "unknown partition"
   
   # include partition ID in the disk name
   PART_NAME="part${PART}"
   DISK_NAME="$DISK_NAME-$PART_NAME"
fi

# get disk UUID, LABEL, PARTUUID, partition table UUID, partition table type, FS type
UUID=
LABEL=
PARTUUID=
PARTLABEL=
PTUUID=
PTTYPE=
TYPE=
USAGE=
PART_ENTRY_SCHEME=
PART_ENTRY_UUID=
PART_ENTRY_TYPE=
PART_ENTRY_NUMBER=
PART_ENTRY_OFFSET=
PART_ENTRY_SIZE=
PART_ENTRY_DISK=

# TODO: busybox blkid?
if [ -x /sbin/blkid ]; then 
   if [ -z "$(echo "$VDEV_PATH" | /bin/egrep "sr.*")" ]; then 
      eval $(/sbin/blkid -p -o export "$VDEV_MOUNTPOINT/$VDEV_PATH")
   fi
else
   vdev_warn "Could not find blkid in /sbin/blkid.  $VDEV_MOUNTPOINT/disk/by-*/ symlinks will not be added."
fi

# get disk WWN, if set 
WWN=
if [ -n "$DISK_WWN" ]; then
   WWN="wwn-$DISK_WWN"

   if [ -n "$PART_NAME" ]; then
      WWN="$WWN-$PART_NAME"
   fi
fi

  
# add the disk links
if [ -n "$DISK_NAME" ]; then
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
PVS_RC=
LVM2_PV_UUID=

if [ -x /sbin/pvs ]; then 
   PVS="/sbin/pvs --nameprefixes --noheadings $VDEV_MOUNTPOINT/$VDEV_PATH"
   eval $($PVS -o pv_uuid)
   PVS_RC=$?
else
   vdev_warn "Could not find pvs in /sbin/pvs.  LVM physical volume symlinks in $VDEV_MOUNTPOINT/disk/by-id will not be created."
fi

if [ $PVS_RC -eq 0 -a -n "$LVM2_PV_UUID" ]; then 
   
   # this is a PV
   vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/lvm-pv-uuid-$LVM2_PV_UUID" "$VDEV_METADATA"
fi

# set ownership and bits 
vdev_permissions root:disk 0660 "$VDEV_MOUNTPOINT/$VDEV_PATH"

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
VDEV_WWN=

ALL_PROPS="VDEV_BUS VDEV_SERIAL VDEV_SERIAL_SHORT VDEV_REVISION VDEV_PART_ENTRY_DISK VDEV_PART_ENTRY_NUMBER VDEV_PART_ENTRY_OFFSET VDEV_PART_ENTRY_SIZE VDEV_PART_ENTRY_SCHEME VDEV_PART_ENTRY_TYPE VDEV_PART_TABLE_TYPE VDEV_PART_ENTRY_UUID VDEV_FS_TYPE VDEV_FS_USAGE VDEV_TYPE VDEV_MAJOR VDEV_MINOR VDEV_OS_SUBSYSTEM VDEV_OS_DEVTYPE VDEV_OS_DEVPATH VDEV_OS_DEVNAME $VDEV_PROPERTIES"

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

esac

# set all device properties
vdev_add_properties "$VDEV_METADATA" $ALL_PROPS

exit 0
