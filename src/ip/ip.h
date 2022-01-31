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

#ifndef ANPNETSTACK_IP_H
#define ANPNETSTACK_IP_H

#include "systems_headers.h"
#include "subuff.h"
#include "ethernet.h"

//https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
// IP protocol numbers
#define IPP_NUM_ICMP     0x01
#define IPP_NUM_IP_in_IP   0x04 // we are doing IP in IP tunning
#define IPP_TCP    0x06

#define DEBUG_IP
#ifdef DEBUG_IP
#define debug_ip_hdr(msg, hdr)                                                \
    do {                                                                \
        printf("IP (HDR %lu) "msg" (ihl: %hhu version: %hhu tos: %hhu "   \
                    "len %hu id: %hu frag_offset: %hu ttl: %hhu " \
                    "proto: %hhu csum: %hx " \
                    "saddr: %hhu.%hhu.%hhu.%hhu daddr: %hhu.%hhu.%hhu.%hhu)\n", \
                    IP_HDR_LEN, \
                    hdr->ihl,                                           \
                    hdr->version, hdr->tos, hdr->len, hdr->id,          \
                    hdr->frag_offset, hdr->ttl, hdr->proto, hdr->csum,   \
                    hdr->saddr >> 24, hdr->saddr >> 16, hdr->saddr >> 8, hdr->saddr >> 0, \
                    hdr->daddr >> 24, hdr->daddr >> 16, hdr->daddr >> 8, hdr->daddr >> 0); \
    } while (0)

#define debug_ip(msg, args...) do {\
    printf("DEBUG_IP: (%s, %d) "msg, __FUNCTION__, __LINE__, ## args);\
}while(0);

#else
#define debug_ip_hdr(msg, hdr)
#define debug_ip(msg, args...)
#endif

// header references: https://www.blackmagicboxes.com/?page_id=237
struct iphdr {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag_offset;
    uint8_t ttl;
    uint8_t proto;
    uint16_t csum;
    uint32_t saddr;
    uint32_t daddr;
    uint8_t data[];
} __attribute__((packed));

#define IP_HDR_LEN sizeof(struct iphdr)
#define IP_PAYLOAD_LEN(_ip) (_ip->len - (_ip->ihl * 4))
#define IP_HDR_FROM_SUB(_sub) (struct iphdr *)(_sub->head + ETH_HDR_LEN);

int ip_rx(struct subuff *);
int ip_output(uint32_t dst_ip_addr, struct subuff *);

#endif //ANPNETSTACK_IP_H
