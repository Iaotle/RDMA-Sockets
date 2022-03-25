#!/bin/bash
set -ex
prog="$1"
shift
LD_PRELOAD="/home/viv600/anptest/lib/libanpnetstack.so.1.0.2" "$prog" "$@"

