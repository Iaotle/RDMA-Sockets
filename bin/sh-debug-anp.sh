#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi
set -ex
prog="$1"
shift

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

gdb \
  -ex "set solib-search-path /usr/local/lib/" \
  -ex "set env ld_library_path=$SCRIPT_DIR/../src" \
  -ex "set follow-fork-mode parent" \
  -ex "set exec-wrapper env 'LD_PRELOAD=$(gcc -print-file-name=libasan.so):/usr/local/lib/libanpnetstack.so'" \
  --args "$prog" "$@"