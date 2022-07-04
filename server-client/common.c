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

// pretty prints message size
char* msgSize(int nbytes) {
    char* str = (char*) malloc(sizeof(char) * 7);

    if (nbytes < KILOBYTE)
        sprintf(str, "%d B\0", nbytes);
    else if (nbytes < MEGABYTE)
        sprintf(str, "%d KB\0", nbytes / KILOBYTE);
    else if (nbytes < GIGABYTE)
        sprintf(str, "%d MB\0", nbytes / MEGABYTE);
    else
        sprintf(str, "%d GB\0", nbytes / GIGABYTE);

    return str;
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
        buf[i] = 'a';
    }
}

const int match_pattern(const unsigned char *buf, int size) {
    unsigned char start = PATTERN_START;
    for (unsigned int i = 0; i < size; i++) {
        if ((0xFFu & buf[i]) != ((start + i) & 0xFFu)) {
            printf(ANSI_COLOR_RED "<NO MATCH> BUFFERS DO NOT MATCH AT INDEX %i, 0x%x == 0x%x\n" ANSI_COLOR_RESET, i, buf[i], ((start + i) & 0xFFu));
            return -1;
        };
    }
    printf(ANSI_COLOR_GREEN "<OK MATCH>\n" ANSI_COLOR_RESET);
    return 0;
}

const int match_pattern2(const unsigned char *buf, int size) {
    unsigned char start = PATTERN_START;
    for (unsigned int i = 0; i < size; i++) {
        if (buf[i] != 'a') {
            printf(ANSI_COLOR_RED "<NO MATCH> BUFFERS DO NOT MATCH AT INDEX %i, 0x%x == 0x%x\n" ANSI_COLOR_RESET, i, buf[i], 'a');
            return -1;
        };
    }
    printf(ANSI_COLOR_GREEN "<OK MATCH>\n" ANSI_COLOR_RESET);
    return 0;
}

// Time diff function copied from https://stackoverflow.com/questions/17705786/getting-negative-values-using-clock-gettime
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

int run_test(int fd, void (*function)(int fd, char *buffer, int size), char *buffer, int size, int num_iter) {
    double avg_latency = 0;
    double avg_bw = 0;
    struct timespec timediff;
    struct timespec start, end;
    function(fd, buffer, size); // warmup

    for (int j = 0; j < NUM_TESTS; j++) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        for (int x = 0; x < num_iter; x++) {
            function(fd, buffer, size);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        timediff = diff(start, end);

        double time_num = timediff.tv_sec + ((double)timediff.tv_nsec) / 1000000000;
        double bps = (double)size * (double)num_iter / time_num;  // bits per second
        double Gbps = bps / GIGABYTE * 8;                         // not metric, base2
        Gbps = bps * 8 / 1000000000.0;
        // printf("Run took: %f seconds, Gbps = " ANSI_COLOR_CYAN "%f\n" ANSI_COLOR_RESET, time_num, Gbps);
        // printf(ANSI_COLOR_CYAN "%f,\n" ANSI_COLOR_RESET, Gbps);
        avg_bw += Gbps;
        // printf("Latency per call: " ANSI_COLOR_CYAN "%f\n" ANSI_COLOR_RESET, time_num / (double)num_iter);
        // printf(ANSI_COLOR_CYAN "%f,\n" ANSI_COLOR_RESET, time_num / (double)num_iter);
        avg_latency += time_num;
    }
	usleep(1000);

    printf("Averages:\nGbps = " ANSI_COLOR_CYAN "%f" ANSI_COLOR_RESET ", Latency " ANSI_COLOR_CYAN "%f\n" ANSI_COLOR_RESET,
           avg_bw / (double)(NUM_TESTS), avg_latency / ((double)NUM_TESTS * (double)num_iter));
}

void send_func(int fd, char *send_buffer, int size) {
    int so_far = 0;
    while (so_far < size) {
        int ret = send(fd, send_buffer + so_far, size - so_far, 0);
        if (0 > ret) {
            printf("Error: send failed with ret %d and errno %d \n", ret, errno);
        }
        so_far += ret;
    }
}

void recv_func(int fd, char *receive_buffer, int size) {
    int so_far = 0;
    while (so_far < size) {
        int ret = recv(fd, receive_buffer + so_far, size - so_far, 0);
        if (0 > ret) {
            printf("Error: recv failed with ret %d and errno %d \n", ret, errno);
        }
        so_far += ret;
    }
}

void send_test(int fd, int size, int num_iter) {
	char* str = msgSize(size);
    printf(ANSI_COLOR_YELLOW "RUNNING %s SEND TEST:\n" ANSI_COLOR_RESET, str);
	free(str);
    char send_buffer[size];
    run_test(fd, send_func, send_buffer, size, num_iter);
}

void recv_test(int fd, int size, int num_iter) {
	char* str = msgSize(size);
    printf(ANSI_COLOR_YELLOW "RUNNING %s RECV TEST:\n" ANSI_COLOR_RESET, str);
	free(str);
    char send_buffer[size];
    run_test(fd, recv_func, send_buffer, size, num_iter);
}
