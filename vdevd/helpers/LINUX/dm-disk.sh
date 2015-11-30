#!/bin/dash


# helper for generating dm block device paths and symlinks.

. "$VDEV_HELPERS/subr.sh"

LVS=
if [ -x /sbin/lvs ]; then 

   LVS="/sbin/lvs -o all --noheadings --nameprefixes"
fi
   

# entry point 
# return 0 on success 
# return 10 on unknown action 
main() {

   local DM_QUERY DM_NAME DM_UUID BLKID_DATA
   local UUID LABEL PARTUUID PARTLABEL PTUUID PTTYPE TYPE
   local LVM2_LV_KERNEL_MAJOR LVM2_LV_KERNEL_MINOR LVM2_VG_NAME LVM2_VG_FULL_NAME LVM2_VG_FMT
   local ALL_PROPS VDEV_PART_TABLE_TYPE VDEV_PART_TABLE_UUID VDEV_FS_TYPE VDEV_DM_NAME VDEV_DM_UUID
   local VDEV_PROPERTIES
   local RC

   VDEV_PROPERTIES=""

   # only /dev/dm-XXX devices
   test -n "$(echo "$VDEV_OS_DEVNAME" | /bin/grep "dm-[0-9]*")" || return 0

   # if removing, just blow away the links
   if [ "$VDEV_ACTION" = "remove" ]; then

      vdev_cleanup "$VDEV_METADATA"
      return 0
   fi

   # make sure we're adding... 
   if [ "$VDEV_ACTION" != "add" ]; then 

      vdev_error "Unknown action '$VDEV_ACTION'"
      return 10
   fi

   DM_QUERY="/sbin/dmsetup -j $VDEV_MAJOR -m $VDEV_MINOR --noudevrules --noudevsync --noheadings --columns info"
   DM_NAME=""
   DM_UUID=""

   if [ -x /sbin/dmsetup ]; then 
      # get name and UUID, for /dev/disk/by-id
      DM_NAME=$($DM_QUERY -oname)
      DM_UUID=$($DM_QUERY -oUUID)
   else
      vdev_warn "Could not find dmsetup in /sbin/dmsetup.  Device mapper symlinks in $VDEV_MOUNTPOINT/disk/by-* and $VDEV_MOUNTPOINT/mapper will not be created."
   fi

   if [ -n "$DM_NAME" ]; then

      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/dm-name-$DM_NAME" "$VDEV_METADATA"
      vdev_symlink "../$VDEV_PATH" "$VDEV_MOUNTPOINT/mapper/$DM_NAME" "$VDEV_METADATA"
   fi

   if [ -n "$DM_UUID" ]; then

      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-id/dm-uuid-$DM_UUID" "$VDEV_METADATA"
   fi

   # get disk UUID, LABEL, PARTUUID, partition table UUID, partition table type, FS type
   UUID=""
   LABEL=""
   PARTUUID=""
   PARTLABEL=""
   PTUUID=""
   PTTYPE=""
   TYPE=""
   BLKID_DATA=""

   if [ -x /sbin/blkid ]; then
      
      # eval "$(/sbin/blkid -o export "$VDEV_MOUNTPOINT/$VDEV_PATH")"
      BLKID_DATA="$(vdev_blkid "$VDEV_MOUNTPOINT/$VDEV_PATH")"
      RC=$?

      if [ $RC -ne 0 ]; then 
         vdev_error "blkid $VDEV_MOUNTPOINT/$VDEV_PATH rc = $RC"
         return $RC
      else 

         # import 
         eval "$BLKID_DATA"
      fi
      
   else
      vdev_warn "Could not find blkid in /sbin/blkid.  Symlinks in $VDEV_MOUNTPOINT/disk/by-* will not be created."
   fi

   if [ -n "$UUID" ]; then
      vdev_symlink "../../$VDEV_PATH" "$VDEV_MOUNTPOINT/disk/by-uuid/$UUID" "$VDEV_METADATA"
   fi

   # create all logical volume links
   if [ -n "$LVS" ]; then 

      $LVS 2>/dev/null | \
      while read lvs_vars; do
         
         LVM2_LV_KERNEL_MAJOR=""
         LVM2_LV_KERNEL_MINOR=""
         LVM2_VG_NAME=""
         LVM2_VG_FULL_NAME=""
         LVM2_VG_FMT=""
      
         eval $lvs_vars
         
         # find this mapped device's LVM info...   
         if [ $LVM2_LV_KERNEL_MAJOR -ne $VDEV_MAJOR ]; then 
            continue
         fi

         if [ $LVM2_LV_KERNEL_MINOR -ne $VDEV_MINOR ]; then 
            continue
         fi

         # only lvm2-formatted supported at this time...
         if [ "$LVM2_VG_FMT" != "lvm2" ]; then 
            vdev_warn "Only lvm2-formatted volumes are supported"
            continue
         fi

         # create the LVM link for this mapped device
         /bin/mkdir -p "$VDEV_MOUNTPOINT/$LVM2_VG_NAME"
         vdev_symlink "../$VDEV_PATH" "$VDEV_MOUNTPOINT/$LVM2_VG_NAME/$LVM2_LV_NAME" "$VDEV_METADATA"

      done
   else
      
      vdev_warn "Could not find lvs in /sbin/lvs.  Logical volume symlinks will not be created."
   fi

   # set ownership and bits 
   vdev_permissions $VDEV_VAR_DM_DISK_OWNER:$VDEV_VAR_DM_DISK_GROUP $VDEV_VAR_DM_DISK_MODE "$VDEV_MOUNTPOINT/$VDEV_PATH"

   # device properties 
   VDEV_PART_TABLE_TYPE="$PTTYPE"
   VDEV_PART_TABLE_UUID="$PTUUID"
   VDEV_FS_TYPE="$TYPE"
   VDEV_DM_NAME="$DM_NAME"
   VDEV_DM_UUID="$DM_UUID"

   ALL_PROPS="VDEV_PART_TABLE_TYPE VDEV_PART_TABLE_UUID VDEV_FS_TYPE VDEV_DM_NAME VDEV_DM_UUID $VDEV_PROPERTIES"

   vdev_add_properties "$VDEV_METADATA" $ALL_PROPS
   return 0
}

if [ $VDEV_DAEMONLET -eq 0 ]; then 
   main 
   exit $?
fi
