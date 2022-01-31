#!/bin/bash
set -ex 
sudo mknod /dev/net/tap c 10 200
sudo chmod 0666 /dev/net/tap
