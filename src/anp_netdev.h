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

#ifndef ANPNETSTACK_ANP_NETDEV_H
#define ANPNETSTACK_ANP_NETDEV_H

#include <stdint.h>
#include "subuff.h"

extern volatile bool stop;

struct eth_hdr;

struct anp_netdev {
    uint32_t addr;
    uint8_t addr_len;
    uint8_t hwaddr[6];
    uint32_t mtu;
};

void client_netdev_init();
int netdev_transmit(struct subuff *skb, uint8_t *dst, uint16_t ethertype);
struct anp_netdev* netdev_get(uint32_t sip);
void *netdev_rx_loop();
void free_netdev();

#endif //ANPNETSTACK_ANP_NETDEV_H
