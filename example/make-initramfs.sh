#!/bin/sh -e

BUILD_INITRAMFS=build/initramfs
VDEV_INITRAMFS_FILES=initramfs
MKINITRAMFS=../tools/initramfs/mkinitramfs

trap "rm -rf \"$BUILD_INITRAMFS\"" 0 1 2 3 15

mkdir -p $BUILD_INITRAMFS
mkdir -p $BUILD_INITRAMFS/vdev
mkdir -p $BUILD_INITRAMFS/init.d

test -d /usr/share/initramfs-tools || (echo "You must install the initramfs-tools package" && exit 1)
test -e /sbin/vdevd || (echo "You must install vdevd first" && exit 1)
test -e /etc/vdev || (echo "You must install vdev's /etc files first" && exit 1)

# copy over initramfs base files, but skip the udev ones
for initramfs_file in $(find /usr/share/initramfs-tools); do

   if [ -d $initramfs_file ]; then 
      continue
   fi

   if [ -n "$(echo $initramfs_file | grep "udev")" ]; then 
      # skip udev file 
      continue
   fi

   base_file=${initramfs_file##/usr/share/initramfs-tools/}
   
   mkdir -p $BUILD_INITRAMFS/$(dirname $base_file)
   cp -a $initramfs_file $BUILD_INITRAMFS/$base_file

done

# copy over vdev's initramfs files 
cp -a $VDEV_INITRAMFS_FILES/* $BUILD_INITRAMFS/

# build it!
echo "Generating initrd.img-`uname -r`"
$MKINITRAMFS -t $BUILD_INITRAMFS -o initrd.img-`uname -r`

exit 0

