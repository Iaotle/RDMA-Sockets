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

#ifndef ANPNETSTACK_ARP_H
#define ANPNETSTACK_ARP_H

#include "ethernet.h"
#include "linklist.h"
#include "anp_netdev.h"
#include "subuff.h"

#define ARP_ETHERNET    0x0001
#define ARP_IPV4        0x0800

//https://en.wikipedia.org/wiki/Address_Resolution_Protocol
#define ARP_REQUEST     0x0001
#define ARP_REPLY       0x0002


#define ARP_CACHE_LEN   32
#define ARP_FREE        0
#define ARP_WAITING     1
#define ARP_RESOLVED    2

#define ARP_DEBUG
#ifdef ARP_DEBUG
#define debug_arp(str, hdr)                                               \
    do {                                                                \
        printf("arp "str" (hwtype: %hu, protype: %.4hx, "          \
                    "hwsize: %d, prosize: %d, opcode: %.4hx)\n",         \
                    hdr->hwtype, hdr->protype, hdr->hwsize,             \
                    hdr->prosize, hdr->opcode);                         \
    } while (0)

#define debug_arp_payload(str, data)                                          \
    do {                                                                \
        printf("arp data "str" (src_mac: %.2hhx:%.2hhx:%.2hhx:%.2hhx"  \
                    ":%.2hhx:%.2hhx, src_ip: %hhu.%hhu.%hhu.%hhu, dst_mac: %.2hhx:%.2hhx" \
                    ":%.2hhx:%.2hhx:%.2hhx:%.2hhx, dst_ip: %hhu.%hhu.%hhu.%hhu) \n", \
                    data->src_mac[0], data->src_mac[1], data->src_mac[2], data->src_mac[3], \
                    data->src_mac[4], data->src_mac[5], data->src_ip >> 24, data->src_ip >> 16, \
                    data->src_ip >> 8, data->src_ip >> 0, data->dst_mac[0], data->dst_mac[1], \
                    data->dst_mac[2], data->dst_mac[3], data->dst_mac[4], data->dst_mac[5], \
                    data->dst_ip >> 24, data->dst_ip >> 16, data->dst_ip >> 8, data->dst_ip >> 0); \
    } while (0)

#define debug_arp_cache(str, entry) \
    do { \
    printf("arp cache "str" (hwtype: %hu, src_ip: %hhu.%hhu.%hhu.%hhu, " \
    "src_mac: %.2hhx:%.2hhx:%.2hhx:%.2hhx:%.2hhx:%.2hhx, state: %d)\n", entry->hwtype, \
        entry->src_ip >> 24, entry->src_ip >> 16, entry->src_ip >> 8, entry->src_ip >> 0, \
        entry->src_mac[0], entry->src_mac[1], entry->src_mac[2], entry->src_mac[3], entry->src_mac[4], \
                entry->src_mac[5], entry->state); \
    } while (0)
#else
#define debug_arp(str, hdr)
#define debug_arp_payload(str, data)
#define debug_arp_cache(str, entry)
#endif

struct arp_ipv4
{
    uint8_t src_mac[6];
    uint32_t src_ip;
    uint8_t dst_mac[6];
    uint32_t dst_ip;
} __attribute__((packed));

struct arp_hdr
{
    uint16_t hwtype;
    uint16_t protype;
    uint8_t hwsize;
    uint8_t prosize;
    uint16_t opcode;
    uint8_t  data[];
} __attribute__((packed));

struct arp_cache_entry
{
    struct list_head list;
    unsigned int state;
    struct arp_ipv4 arpIpv4;
};

void arp_init();
void free_arp();
void arp_rx(struct subuff *skb);
void arp_reply(struct subuff *skb, struct anp_netdev *netdev);
int arp_request(uint32_t src_ip, uint32_t dst_ip, struct anp_netdev *netdev);
unsigned char* arp_get_hwaddr(uint32_t src_ip);

static inline struct arp_hdr *arp_hdr(struct subuff *sub)
{
    return (struct arp_hdr *)(sub->head + ETH_HDR_LEN);
}

#define ARP_HDR_LEN sizeof(struct arp_hdr)
#define ARP_DATA_LEN sizeof(struct arp_ipv4)

#endif //ANPNETSTACK_ARP_H
