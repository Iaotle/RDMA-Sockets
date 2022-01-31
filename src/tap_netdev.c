/*
 * Copyright [2020] [Animesh Trivedi]
 *
 * This code is part of the Advanced Network Programming (ANP) course
 * at VU Amsterdam.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *        http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*
 * In this file all the TUN/TAP device management related functions are defined.
 */

#include "tap_netdev.h"
#include "utilities.h"
#include "config.h"

static struct tap_netdev *_tdev = NULL;

char *get_tdev_name(){
    return _tdev->devname;
}
/*
 * Taken from Kernel Documentation/networking/tuntap.txt
 */
static int tdev_alloc(struct tap_netdev *dev)
{
    struct ifreq ifr;
    int fd, err;
    fd = open("/dev/net/tap", O_RDWR);
    if( 0 > fd ) {
        printf("Cannot open any TUN/TAP dev, errno %d \n", errno);
        printf("Make sure one exists, otherwise just create one with as shown below \n >sudo mknod /dev/net/tap c 10 200\n");
        printf("Alternatively you can check the bin directory, sh-make-tun-dev.sh\n");
        exit(1);
    }
    _clear_var(ifr);

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *        IFF_NO_PI - Do not provide packet information
     * In this project we want raw access (no additional information) to the Ethernet frames
     * on the TAP device (Ethernet), not the TUN (ip level)
     */
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if( NULL != dev->devname ) {
        // there is name passed, then use it
        strncpy(ifr.ifr_name, dev->devname, IFNAMSIZ);
    }
    err = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if( 0 > err ){
        printf("ERR: Could not ioctl tun device, errno %d err %d \n", errno, err);
        close(fd);
        return err;
    }
    // dst, src
    strcpy(dev->devname, ifr.ifr_name);
    dev->tun_fd = fd;
    return err;
}


void tdev_init(void)
{
    _tdev = calloc(sizeof(struct tap_netdev), 1);
    int ret = -1;
    if(NULL == _tdev){
        printf("error null value, illegal argument \n");
        exit(-EINVAL);
    }
    _tdev->devname = calloc(1, IFNAMSIZ);
    ret = tdev_alloc(_tdev);
    if(0 != ret){
        printf("ERROR device alloc failed, ret %d, errno %d \n", ret, errno);
        exit(-ret);
    }
    printf("tap device OK, %s \n", _tdev->devname);
    // bring the device up
    ret = run_bash_command("ip link set dev %s up", _tdev->devname);
    if(0 != ret){
        printf("ERROR failed getting the device up, errno %d \n", errno);
        exit(-ret);
    }
    printf("OK: device should be up now, %s \n", ANP_SUBNET_TAP);
    //2. set the CIDR routing
    ret = run_bash_command("ip route add dev %s %s", _tdev->devname, ANP_SUBNET_TAP);
    if (0 != ret) {
        printf("ERROR failed setting the device route %s, errno %d \n", ANP_SUBNET_TAP, errno);
        exit(-ret);
    }
    printf("OK: setting the device route, %s \n", ANP_SUBNET_TAP);
    // 3. setup the device address (MUST be last, the ordering is important).
    ret = run_bash_command("ip address add dev %s local %s", _tdev->devname, ANP_IP_TAP_DEV);
    if (0 != ret) {
        printf("ERROR failed setting the device address, %s errno %d \n", ANP_IP_TAP_DEV, errno);
        exit(-ret);
    }
    printf("OK: setting the device address %s \n", ANP_IP_TAP_DEV);
}

int tdev_read(char *buf, int len)
{
    return read(_tdev->tun_fd, buf, len);
}

int tdev_write(char *buf, int len)
{
    return write(_tdev->tun_fd, buf, len);
}