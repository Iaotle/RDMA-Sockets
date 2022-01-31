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

#include "ip.h"
#include "systems_headers.h"
#include "utilities.h"
#include "config.h"
#include "icmp/icmp.h"
#include "tcp/tcp.h"

int ip_rx(struct subuff *sub)
{
    struct iphdr *ih = IP_HDR_FROM_SUB(sub);
    uint16_t csum = -1;

    if (ih->version != IPP_NUM_IP_in_IP) {
        printf("IP packet is not IP\n");
        goto drop_pkt;
    }

    if (ih->ihl < 5) {
        printf("IP packet header is too short, expected atleast 20 bytes, got %d \n", ((ih->ihl)<<2));
        goto drop_pkt;
    }

    if (ih->ttl == 0) {
        printf("ERROR: zero time to live, ttl, dropping packet \n");
        goto drop_pkt;
    }

    csum = do_csum(ih, ih->ihl * 4, 0);

    if (csum != 0) {
        printf("Error: invalid checksum, dropping packet");
        goto drop_pkt;
    }

    ih->saddr = ntohl(ih->saddr);
    ih->daddr = ntohl(ih->daddr);
    ih->len = ntohs(ih->len);
    ih->id = ntohs(ih->id);


    switch (ih->proto) {
        case IPP_NUM_ICMP:
            debug_ip_hdr("in", ih);
            debug_ip("incoming ICMP packet\n");
            icmp_rx(sub);
            return 0;
        case IPP_TCP:
            //debug_ip_hdr("in", ih);
            tcp_rx(sub);
            return 0;
        default:
            #ifdef ANP_PRINT_UNKNOWN_PACKETS
            debug_ip_hdr("in", ih);
            printf("Error: Unknown IP header proto %d \n", ih->proto);
            #endif
            goto drop_pkt;
    }
    drop_pkt:
    free_sub(sub);
    return 0;
}

