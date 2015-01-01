#!/bin/bash

if ! [ $1 ]; then
   echo "Usage: $0 PACKAGE_ROOT"
   exit 1
fi

ROOT=$1
NAME="vdev"
VERSION="0.$(date +%Y\%m\%d\%H\%M\%S)"

DEPS="libpstat libfskit-fuse"

DEPARGS=""
for pkg in $DEPS; do
   DEPARGS="$DEPARGS -d $pkg"
done

source /usr/local/rvm/scripts/rvm

rm -f $NAME-0*.deb

fpm --force -s dir -t deb -a $(uname -m) -v $VERSION -n $NAME $DEPARGS -C $ROOT --license "GPLv3+/ISC" --maintainer "Jude Nelson <judecn@gmail.com>" --url "https://github.com/jcnelson/runfs" --description "Virtual device filesystem." $(ls $ROOT)

