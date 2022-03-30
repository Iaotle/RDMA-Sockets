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

#ifndef ATR_TUNTAP_UTILS_H
#define ATR_TUNTAP_UTILS_H

#include "systems_headers.h"
#include <execinfo.h>

#define _clear_var(addr) memset(&(addr), 0, sizeof(addr))

#define CMDBUFLEN 128

#define MIN(x, y) ((x) < (y) ? (x) : (y))

int run_bash_command(char *cmd, ...);
uint16_t do_csum(void *addr, int count, int start_sum);
uint32_t ip_str_to_n32(const char *addr);
uint32_t ip_str_to_h32(const char *addr);
void u32_ip_to_str(char *, uint32_t daddr);
void print_trace(void);
int do_tcp_csum(uint8_t *data, int length, uint16_t protocol, uint32_t saddr, uint32_t daddr);

#define ANP_MIN(a, b) (a < b ? a : b)

#endif //ATR_TUNTAP_UTILS_H
