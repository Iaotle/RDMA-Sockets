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

#include <stdbool.h>
#include "common.h"

#define PATTERN_START 0xAA;

int get_addr(char *dst, struct sockaddr *addr)
{
    struct addrinfo *res;
    int ret = -1;
    ret = getaddrinfo(dst, NULL, NULL, &res);
    if (ret) {
        printf("Error: getaddrinfo failed - invalid hostname or IP address\n");
        return ret;
    }
    memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(res);
    return ret;
}

void write_pattern(char *buf, int size){
    // write a pattern
    unsigned char start = PATTERN_START;
    for(unsigned int i = 0; i < size; i++){
        buf[i] = (start + i) & 0xFFu;
    }
}

const char * match_pattern(const unsigned char *buf, int size){
    unsigned char start = PATTERN_START;
    for(unsigned int i = 0; i < size; i++){
        if( (0xFFu & buf[i]) != ((start + i) & 0xFFu)){

            printf("wrong pattern here ? returning %s , index %d buf 0x%x patt 0x%x \n", " <_DO_NOT match> ", i, buf[i], ((start + i) & 0xFFu));
            return " < _DO_NOT match > ";
        };
    }
    return " < OK, matched > ";
}