#!/bin/bash

if ! [ $1 ]; then
   echo "Usage: $0 PACKAGE_ROOT"
   exit 1
fi

which fpm || (echo "Could not find the fpm tool in your PATH."; exit 1)

ROOT=$1
NAME="vdev"
VERSION="0.$(date +%Y\%m\%d\%H\%M\%S)"
MAINTAINER="judecn@devuan.org"
URL="https://git.devuan.org/pkgs-utopia-substitution/vdev"
DESC="A virtual device manager, alpha unstable build.  DO NOT INSTALL ON A PRODUCTION SYSTEM.  DO NOT INSTALL UNLESS YOU KNOW HOW TO FIX A BROKEN SYSTEM."

DEPS=$(cat ../dependencies.txt)

DEPARGS=""
for pkg in $DEPS; do
   DEPARGS="$DEPARGS -d $pkg"
done

source /usr/local/rvm/scripts/rvm

#rm -f $NAME-0*.deb

echo "fpm --force -s dir -t deb -a $(uname -m) -v $VERSION -n $NAME $DEPARGS -C $ROOT --license "GPLv3+/ISC" --maintainer $MAINTAINER --url $URL --description $DESC $(ls $ROOT)"

