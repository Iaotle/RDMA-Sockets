#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# Find correct lib path
FILE=/usr/local/lib/libanpnetstack.so
if [ ! -f "$FILE" ]; then
	FILE=/usr/local/lib64/libanpnetstack.so
fi

set -ex
prog="$1"
shift
LD_PRELOAD="$FILE" "$prog" "$@"

