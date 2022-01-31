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

#include <assert.h>
#include "anp_netdev.h"
#include "systems_headers.h"
#include "ethernet.h"
#include "subuff.h"
#include "utilities.h"
#include "tap_netdev.h"
#include "config.h"
#include "arp.h"
#include "ip.h"

struct anp_netdev *cdev_lo;
struct anp_netdev *cdev_ext;

volatile bool stop = false;

static struct anp_netdev *netdev_alloc(char *addr, char *hwaddr, uint32_t mtu)
{
    struct anp_netdev *dev = calloc(1, sizeof(struct anp_netdev));
    if( NULL == dev){
        printf("Error: dev calloc failed \n");
        return NULL;
    }
    dev->addr = ip_str_to_h32(addr);
    sscanf(hwaddr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &dev->hwaddr[0],
           &dev->hwaddr[1],
           &dev->hwaddr[2],
           &dev->hwaddr[3],
           &dev->hwaddr[4],
           &dev->hwaddr[5]);
    dev->addr_len = 6;
    dev->mtu = mtu;
    return dev;
}

void client_netdev_init()
{
    cdev_ext = netdev_alloc(ANP_IP_CLIENT_EXT, ANP_MAC_CLIENT_EXT, ANP_MTU_15);
    cdev_lo = netdev_alloc(ANP_IP_LO, ANP_MAC_CLIENT_LO, ANP_MTU_15);
}


static int get_mac(const char *iface, uint8_t *mac)
{
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if( fd < 0){
        printf(" fd failed \n");
        exit(-1);
    }
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);
    int ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
    if( ret < 0 ){
        printf("ioctl failed \n");
        exit(0);
    }
    close(fd);
    for(int i = 0; i < 6; i++){
        mac[i] = ifr.ifr_hwaddr.sa_data[i];
    }
    return 0;
}


int netdev_transmit(struct subuff *sub, uint8_t *dst_hw, uint16_t ethertype)
{
    struct anp_netdev *dev = sub->dev;
    sub_push(sub, ETH_HDR_LEN);
    struct eth_hdr *hdr = (struct eth_hdr *)sub->data;
    int ret = 0;
    memcpy(hdr->dmac, dst_hw, dev->addr_len);
    memcpy(hdr->smac, dev->hwaddr, dev->addr_len);
    hdr->ethertype = htons(ethertype);
    ret = tdev_write((char *)sub->data, sub->len);
    return ret;
}

static int process_packet(struct subuff *sub)
{
    struct eth_hdr *hdr = eth_hdr(sub);
    switch (hdr->ethertype) {
        case ETH_P_ARP:
            // TODO: ANP milestone 1 -- run and test ARP implementation
            arp_rx(sub);
            break;
        case ETH_P_IP:
            ip_rx(sub);
            break;
        case ETH_P_IPV6:
            printf("Error: Did you forget to disable the ipv6 support?\n");
        default:
            printf("Error: Unsupported ethertype %x\n", hdr->ethertype);
            free_sub(sub);
            break;
    }

    return 0;
}

void *netdev_rx_loop()
{
    int ret;
    while (!stop) {
        // The max size of ethernet packet over 1500 MTU (including additional headers */
        // https://searchnetworking.techtarget.com/answer/Minimum-and-maximum-Ethernet-frame-sizes
        struct subuff *sub = alloc_sub(ANP_MTU_15_MAX_SIZE);
        ret = tdev_read((char *)sub->data, ANP_MTU_15_MAX_SIZE);
        if (ret < 0) {
            printf("Error in reading the tap device, %d and errno %d \n", ret, errno);
            free_sub(sub);
            return NULL;
        }
        // whatever we have received, pass it along
        process_packet(sub);
    }
    return NULL;
}

struct anp_netdev* netdev_get(uint32_t sip)
{
    if (cdev_ext->addr == sip) {
        return cdev_ext;
    } else {
        return NULL;
    }
}

void free_netdev()
{
    free(cdev_lo);
    free(cdev_ext);
}
