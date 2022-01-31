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

#include "utilities.h"
#include "systems_headers.h"

int run_bash_command(char *cmd, ...){
    va_list params;
    char exe_buffer[CMDBUFLEN];
    va_start(params, cmd);
    vsnprintf(exe_buffer, CMDBUFLEN, cmd, params);
    va_end(params);
    printf("Executing : %s \n", exe_buffer);
    return system(exe_buffer);
}

static uint32_t sum_every_16bits(void *addr, int count)
{
    register uint32_t sum = 0;
    uint16_t * ptr = addr;

    while( count > 1 )  {
        /*  This is the inner loop */
        sum += * ptr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if( count > 0 )
        sum += * (uint8_t *) ptr;

    return sum;
}

uint16_t do_csum(void *addr, int count, int start_sum)
{
    /* Compute Internet Checksum for "count" bytes
     *         beginning at location "addr".
     * Taken from https://tools.ietf.org/html/rfc1071
     */
    uint32_t sum = start_sum;

    sum += sum_every_16bits(addr, count);

    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}

int do_tcp_csum(uint8_t *data, int length, uint16_t protocol, uint32_t saddr, uint32_t daddr)
{
    uint32_t sum = 0;

    sum += saddr;
    sum += daddr;
    sum += htons(protocol);
    sum += htons(length);
    return do_csum(data, length, sum);
}

uint32_t ip_str_to_n32(const char *addr){
    uint32_t dst = 0;
    if (inet_pton(AF_INET, addr, &dst) != 1) {
        printf("Error: bad IP %s \n", addr);
        exit(1);
    }
    return dst;
}

uint32_t ip_str_to_h32(const char *addr)
{
    //network to host formatting
    return ntohl(ip_str_to_n32(addr));
}

void u32_ip_to_str(char * str, uint32_t daddr){

    uint8_t bytes[4];
    bytes[0] = daddr & 0xFF;
    bytes[1] = (daddr >> 8) & 0xFF;
    bytes[2] = (daddr >> 16) & 0xFF;
    bytes[3] = (daddr >> 24) & 0xFF;
    printf ("%s %u.%u.%u.%u\n", str, bytes[3], bytes[2], bytes[1], bytes[0]);
}

void print_trace(void) {
    char **strings;
    size_t i, size;
    enum Constexpr { MAX_SIZE = 1024 };
    void *array[MAX_SIZE];
    size = backtrace(array, MAX_SIZE);
    strings = backtrace_symbols(array, size);
    for (i = 0; i < size; i++)
        printf("%s\n", strings[i]);
    puts("");
    free(strings);
}