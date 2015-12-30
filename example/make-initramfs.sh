#!/bin/sh -e

BUILD_INITRAMFS=../build/initramfs
BUILD_INITRAMFS_CONFDIR=../build/etc/initramfs-tools
VDEV_INITRAMFS_FILES=initramfs
MKINITRAMFS=../tools/initramfs/mkinitramfs

KERNEL_VERSION=`uname -r`
if [ -n "$1" ]; then 
   if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then 
      echo "Usage: $0 [-h|--help] [kernel_version]" >&2
      exit 0
   fi

   KERNEL_VERSION="$1"
fi

trap "rm -rf \"$BUILD_INITRAMFS\" \"$BUILD_INITRAMFS_CONFDIR\"" 0 1 2 3 15

mkdir -p "$BUILD_INITRAMFS"

test -d /usr/share/initramfs-tools || (echo "You must install the initramfs-tools package" && exit 1)
test -e /sbin/vdevd || (echo "You must install vdevd first" && exit 1)
test -e /etc/vdev || (echo "You must install vdev's /etc files first" && exit 1)

# copy over initramfs base files, but skip the udev ones
for initramfs_file in $(find /usr/share/initramfs-tools); do

   if [ -d "$initramfs_file" ]; then 
      continue
   fi

   if [ -n "$(echo "$initramfs_file" | grep "udev")" ]; then 
      # skip udev file 
      continue
   fi

   base_file=${initramfs_file##/usr/share/initramfs-tools/}
   
   mkdir -p "$BUILD_INITRAMFS/$(dirname "$base_file")"
   cp -a "$initramfs_file" "$BUILD_INITRAMFS/$base_file"

done

# copy over initramfs conf files, but skip the udev ones
for initramfs_file in $(find /etc/initramfs-tools); do

   if [ -d "$initramfs_file" ]; then 
      continue
   fi

   if [ -n "$(echo "$initramfs_file" | grep "udev")" ]; then 
      # skip udev file 
      continue
   fi

   base_file=${initramfs_file##/etc/initramfs-tools/}
   
   mkdir -p "$BUILD_INITRAMFS_CONFDIR/$(dirname "$base_file")"
   cp -a "$initramfs_file" "$BUILD_INITRAMFS_CONFDIR/$base_file"

done

# copy over vdev's init and modules... 
cp -a "$VDEV_INITRAMFS_FILES/init" "$BUILD_INITRAMFS"

# copy over vdev's initramfs files to conf 
cp -a "$VDEV_INITRAMFS_FILES"/* "$BUILD_INITRAMFS_CONFDIR"/
rm "$BUILD_INITRAMFS_CONFDIR"/init

# build it!
echo "Generating initrd.img-$KERNEL_VERSION"
"$MKINITRAMFS" -t "$BUILD_INITRAMFS" -d "$BUILD_INITRAMFS_CONFDIR" -o "initrd.img-$KERNEL_VERSION" "$KERNEL_VERSION"

if [ $? -eq 0 ]; then 
   echo "Generated initrd.img-$KERNEL_VERSION"
   echo ""
   echo "You must install initrd.img-$KERNEL_VERSION via your"
   echo "bootloader.  GRUB2 users can simply overwrite the"
   echo "original initramfs image."
   echo ""
   echo "BE SURE TO BACK UP YOUR ORIGINAL INITRAMFS."
fi

exit 0

