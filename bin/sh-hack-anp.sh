#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi
set -ex
prog="$1"
shift
LD_PRELOAD="/home/viv600/anptest/lib/libanpnetstack.so.1.0.1" "$prog" "$@"

