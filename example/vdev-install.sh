#!/bin/sh -e

PREFIX=/

if [ $# -gt 1 ]; then 
   PREFIX=$1
   shift 1
fi

# make sure we installed vdev 
test -x $PREFIX/etc/init.d/vdev || (echo "You must install vdev first" && exit 1)

update-rc.d -f udev remove
update-rc.d -f udev-finish remove
update-rc.d -f vdev defaults
insserv

exit 0
