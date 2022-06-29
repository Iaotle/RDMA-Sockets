#!/bin/bash
set -ex
prog="$1"
shift
LD_PRELOAD="/home/viv600/anptest/lib/librdmanetstack.so.1.1.0" "$prog" "$@"