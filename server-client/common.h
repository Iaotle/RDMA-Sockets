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

#ifndef SIMPLE_SERVER_CLIENT_COMMON_H
#define SIMPLE_SERVER_CLIENT_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>

#define PORT 43211

#define SMALL_BUFF (1<<12) // 4KiB
#define LARGE_BUFF (1<<17) // 32KiB
#define TEST_BUF_SIZE SMALL_BUFF

int get_addr(char *dst, struct sockaddr *addr);
const char* match_pattern(const unsigned char *buf, int size);
void write_pattern(char *buf, int size);

#endif //SIMPLE_SERVER_CLIENT_COMMON_H
