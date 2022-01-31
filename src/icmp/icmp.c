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

#include "ip/ip.h"
#include "icmp.h"
#include "utilities.h"

void icmp_rx(struct subuff *sub)
{
    struct icmp* icmp_packet = (struct icmp*) (sub->head + ETH_HDR_LEN + IP_HDR_LEN);
    printf("ICMP type: %d, code: %d \n", icmp_packet->type, icmp_packet->code);
    if (icmp_packet->type != ICMP_V4_ECHO){
        goto drop_packet;
    }

    struct iphdr* ip = IP_HDR_FROM_SUB(sub);
    if (do_csum(icmp_packet, IP_PAYLOAD_LEN(ip), 0) != 0){
        goto drop_packet;
    }

    icmp_reply(sub, icmp_packet);
    
    drop_packet:
    free_sub(sub);
}

void icmp_reply(struct subuff *sub, struct icmp* icmp_packet)
{
    struct iphdr* ip = IP_HDR_FROM_SUB(sub);
    uint16_t icmp_packet_size = IP_PAYLOAD_LEN(ip);
    uint16_t total_size = ETH_HDR_LEN + IP_HDR_LEN + icmp_packet_size;

    icmp_packet->type = ICMP_V4_REPLY;
    icmp_packet->checksum = 0;
    icmp_packet->checksum = do_csum(icmp_packet, icmp_packet_size, 0);
    
    sub->protocol = IPP_NUM_ICMP;
    sub_reserve(sub, total_size);
    sub_push(sub, icmp_packet_size);
    ip_output(ip->saddr, sub);
}
