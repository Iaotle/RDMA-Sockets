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

#define PORT 43211

#define KILOBYTE (1 << 10) // 1KiB
#define TWO_KILO (1 << 11) // 2KiB
#define SMALL_BUFF (1 << 12)  // 4KiB
#define LARGE_BUFF (1 << 17)  // 32KiB
#define MEGABYTE (1 << 20) // 1MB
#define EIGHT_MEGABYTES (1 << 23) // 8MB
#define SIXTEEN_MEGABYTES (1 << 24) // 16MB
#define THIRTY_TWO_MEGABYTES (1 << 25) // 32MB
#define ONETWOEIGHT_MEGABYTES (1 << 27) // 128MB
#define TWOFIXESIX_MEGABYTES (1 << 28) // 256MB
#define FIVEONETWO_MEGABYTES (1 << 29) // 512MB
#define GIGABYTE (1 << 30) // 1GB


// Benchmark Parameters:
#define NUM_TESTS 100
#define NUM_ITERATIONS 100
#define INIT_SIZE 2
#define MAX_SIZE GIGABYTE

#define TEST_MESSAGE_SIZE MEGABYTE




int get_addr(char *dst, struct sockaddr *addr);
const int match_pattern(const unsigned char *buf, int size);
const int match_pattern2(const unsigned char *buf, int size);
void write_pattern(char *buf, int size);
void write_pattern2(char *buf, int size);
void send_test(int fd, int size, int num_iter);
void recv_test(int fd, int size, int num_iter);


struct timespec diff(struct timespec start, struct timespec end);


#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#endif  // SIMPLE_SERVER_CLIENT_COMMON_H

