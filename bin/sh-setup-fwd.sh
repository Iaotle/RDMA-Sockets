#!/bin/bash
set -ex
if [ $# -ne 1 ]; then 
	echo "which NIC device should I use to bind the TAP device? "
	echo "it is the same NIC which is connected to outside world, perhaps in Ubuntu-18 is it enp0s3"
	echo "execute ifconfig -a command to check all the NICs" 
	exit 1;
fi 
echo "setting up with $1 , on the tun subet of 10.0.0.0/24" 

sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -I INPUT --source 10.0.0.0/24 -j ACCEPT
sudo iptables -t nat -I POSTROUTING --out-interface $1 -j MASQUERADE
sudo iptables -I FORWARD --in-interface $1 --out-interface tap0 -j ACCEPT
sudo iptables -I FORWARD --in-interface tap0 --out-interface $1 -j ACCEPT

# switch off offload settings
sudo ifconfig lo mtu 1500
sudo ifconfig $1 mtu 1500
sudo ethtool -K $1 tso off lro off gro off gso off
sudo ethtool -K lo tso off lro off gro off gso off