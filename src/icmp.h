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

#ifndef ANPNETSTACK_ICMP_H
#define ANPNETSTACK_ICMP_H

#include "systems_headers.h"
#include "subuff.h"

//https://www.iana.org/assignments/icmp-parameters/icmp-parameters.xhtml

#define ICMP_V4_REPLY           0x00
#define ICMP_V4_ECHO            0x08

// ICMP packet header https://www.researchgate.net/figure/ICMP-packet-structure_fig5_316727741
struct icmp {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint8_t data[];
} __attribute__((packed));

void icmp_rx(struct subuff *sub);
void icmp_reply(struct subuff *sub);

#endif //ANPNETSTACK_ICMP_H
