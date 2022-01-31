interface=$(route | grep default | awk '{print $8}')
echo $interface
./bin/sh-make-tun-dev.sh
./bin/sh-disable-ipv6.sh
./bin/sh-setup-fwd.sh $interface
