/*
 * Copyright [2020] [Animesh Trivedi]
 *
 * Modified by Vadim Isakov
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

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include "./../src/util.h"

#define PORT 43211



// Benchmark Parameters:
#define NUM_TESTS 100
#define NUM_ITERATIONS 100
#define INIT_SIZE 2
#define MAX_SIZE GIGABYTE // (128 * KILOBYTE)

#define TEST_MESSAGE_SIZE MEGABYTE



int get_addr(char *dst, struct sockaddr *addr);
const int match_pattern(const unsigned char *buf, int size);
const int match_pattern2(const unsigned char *buf, int size);
void write_pattern(char *buf, int size);
void write_pattern2(char *buf, int size);
void send_test(int fd, int size, int num_iter);
void recv_test(int fd, int size, int num_iter);


struct timespec diff(struct timespec start, struct timespec end);



#endif  // SIMPLE_SERVER_CLIENT_COMMON_H

