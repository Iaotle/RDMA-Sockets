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

#include "common.h"

#include <stdbool.h>

#define PATTERN_START 0xAA;

int get_addr(char *dst, struct sockaddr *addr) {
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

void write_pattern(char *buf, int size) {
    // write a pattern
    unsigned char start = PATTERN_START;
    for (unsigned int i = 0; i < size; i++) {
        buf[i] = (start + i) & 0xFFu;
        // buf[i] = 'a';
    }
}

void write_pattern2(char *buf, int size) {
    // write a pattern
    unsigned char start = PATTERN_START;
    for (unsigned int i = 0; i < size; i++) {
        // buf[i] = (start + i) & 0xFFu;
        buf[i] = 'a';
    }
}

const int *match_pattern2(const unsigned char *buf, int size) {
    unsigned char start = PATTERN_START;
    for (unsigned int i = 0; i < size; i++) {
        if (buf[i] != 'a') {
			printf(ANSI_COLOR_RED"BUFFERS DO NOT MATCH AT INDEX %i, 0x%x == 0x%x\n"ANSI_COLOR_RESET, i, buf[i], 'a');
            // printf("wrong pattern here ? returning %s , index %d buf 0x%x patt 0x%x \n", " <_DO_NOT match> ", i, buf[i], 'a');
            // return " \033[0;31m< _DO_NOT match >\033[0;37m ";
			return -1;
        };
    }
	printf(ANSI_COLOR_GREEN"<OK MATCH>\n"ANSI_COLOR_RESET);
    return 0; //" \n\033[0;32m< OK, matched >\033[0;37m ";
}

const int *match_pattern(const unsigned char *buf, int size) {
    unsigned char start = PATTERN_START;
    for (unsigned int i = 0; i < size; i++) {
        if ((0xFFu & buf[i]) != ((start + i) & 0xFFu)) {
			printf(ANSI_COLOR_RED"<NO MATCH> BUFFERS DO NOT MATCH AT INDEX %i, 0x%x == 0x%x\n"ANSI_COLOR_RESET, i, buf[i], ((start + i) & 0xFFu));
            // printf("wrong pattern here ? returning %s , index %d buf 0x%x patt 0x%x \n", " <_DO_NOT match> ", i, buf[i], ((start + i) & 0xFFu));
            return -1;
        };
    }
	printf(ANSI_COLOR_GREEN"<OK MATCH>\n"ANSI_COLOR_RESET);
	//printf("\n\033[0;32m< OK, matched >\033[0;37m ");
    return 0; //" \n\033[0;32m< OK, matched >\033[0;37m ";
}

struct timespec diff(struct timespec start, struct timespec end) {
    struct timespec temp;

    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

int run_test(int fd, 
			void (*function)(int fd, char send_buffer[TEST_BUFFER_LENGTH], char receive_buffer[TEST_BUFFER_LENGTH]), 
			char send_buffer[TEST_BUFFER_LENGTH],
			char receive_buffer[TEST_BUFFER_LENGTH]) {

	// double //TODO: make averages
    
	struct timespec start, end;
    
	for (int j = 0; j < NUM_TESTS; j++) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        for (int x = 0; x < NUM_ITERATIONS; x++) {
            function(fd, send_buffer, receive_buffer);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        struct timespec timediff = diff(start, end);
        double time_num = timediff.tv_sec + ((double)timediff.tv_nsec) / 1000000000;
        double Bps = (double)TEST_BUFFER_LENGTH * (double)NUM_ITERATIONS / time_num;  // BYTES per second
        double Gbps = Bps / GIGABYTE * 8;
        printf("Run took: %f seconds, Gbps = " ANSI_COLOR_CYAN "%f\n" ANSI_COLOR_RESET, time_num, Gbps);
		printf("Latency per call: "ANSI_COLOR_CYAN"%f\n"ANSI_COLOR_RESET, (double)NUM_ITERATIONS/time_num);
    }
}

void send_func(int fd, char send_buffer[TEST_BUFFER_LENGTH], char receive_buffer[TEST_BUFFER_LENGTH]) {

    int so_far = 0;
    while (so_far < TEST_BUFFER_LENGTH) {
        int ret = send(fd, send_buffer + so_far, TEST_BUFFER_LENGTH - so_far, 0);
        if (0 > ret) {
            printf("Error: send failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far += ret;
    }
}

void recv_func(int fd, char send_buffer[TEST_BUFFER_LENGTH], char receive_buffer[TEST_BUFFER_LENGTH]) {
    int so_far = 0;
    while (so_far < TEST_BUFFER_LENGTH) {
        int ret = recv(fd, receive_buffer + so_far, TEST_BUFFER_LENGTH - so_far, 0);
        if (0 > ret) {
            printf("Error: recv failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far += ret;
    }
	// match_pattern(receive_buffer, TEST_BUFFER_LENGTH);
}

void send_test(int fd) {
    char send_buffer[TEST_BUFFER_LENGTH], receive_buffer[TEST_BUFFER_LENGTH];
    run_test(fd, send_func, send_buffer, receive_buffer);
}

void recv_test(int fd) {
    char send_buffer[TEST_BUFFER_LENGTH], receive_buffer[TEST_BUFFER_LENGTH];
    run_test(fd, recv_func, send_buffer, receive_buffer);
}

void send_func_sanity(int fd, char send_buffer[TEST_BUFFER_LENGTH], char receive_buffer[TEST_BUFFER_LENGTH]) {

    int so_far = 0;
    while (so_far < TEST_BUFFER_LENGTH) {
        int ret = send(fd, send_buffer + so_far, TEST_BUFFER_LENGTH - so_far, 0);
        if (0 > ret) {
            printf("Error: send failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far += ret;
    }
}

void recv_func_sanity(int fd, char send_buffer[TEST_BUFFER_LENGTH], char receive_buffer[TEST_BUFFER_LENGTH]) {
    int so_far = 0;
    while (so_far < TEST_BUFFER_LENGTH) {
        int ret = recv(fd, receive_buffer + so_far, TEST_BUFFER_LENGTH - so_far, 0);
        if (0 > ret) {
            printf("Error: recv failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far += ret;
    }
	// match_pattern(receive_buffer, TEST_BUFFER_LENGTH);
}


void send_test_sanity(int fd) {
    char send_buffer[TEST_BUFFER_LENGTH], receive_buffer[TEST_BUFFER_LENGTH];
    run_test(fd, send_func_sanity, send_buffer, receive_buffer);
}

void recv_test_sanity(int fd) {
    char send_buffer[TEST_BUFFER_LENGTH], receive_buffer[TEST_BUFFER_LENGTH];
    run_test(fd, recv_func_sanity, send_buffer, receive_buffer);
}