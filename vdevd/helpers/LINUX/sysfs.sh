#!/bin/dash

# helper to store device metadata from sysfs to the OS metadata database.
# in particular:
# * map a device's sysfs path to its metadata directory

. "$VDEV_HELPERS/subr.sh"

SYSFS_PATH="$VDEV_OS_SYSFS_MOUNTPOINT/$VDEV_OS_DEVPATH"
SERIALIZED_PATH="$(vdev_serialize_path "$SYSFS_PATH")"

# remember where this device's metadata directory is
# put it in the *global* metadata directory; as in, /dev/metadata/sysfs/$SERIALIZED_PATH --> $VDEV_METADATA
vdev_symlink "$VDEV_METADATA" "$VDEV_GLOBAL_METADATA/sysfs/$SERIALIZED_PATH" "$VDEV_METADATA"

exit $?