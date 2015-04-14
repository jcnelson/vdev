#!/bin/sh

IFNAME=""
MAC=""

ip link show | \
while read v1 v2 v3; do

   if [ -z "$IFNAME" ]; then 

      # read interface, and then read MAC address
      IFNAME=${v2%%:}

   else
      # read MAC address, emit line, and read ifname again 
      MAC=$v2

      # skip loopback
      if [ "$MAC" != "00:00:00:00:00:00" ]; then 

         echo "$IFNAME mac $MAC"
      fi

      IFNAME=""
      MAC=""
   fi

done
