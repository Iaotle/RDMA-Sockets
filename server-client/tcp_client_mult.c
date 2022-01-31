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

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"

#define MATCH_OK 0
#define MATCH_FAILED 1
#define MATCH_UNDEF 2

#define TPRINTF(str, ...) printf("[Thread %d] " str, cargs->thread_id, ##__VA_ARGS__);

// sudo tcpdump -i wlp2s0 tcp port 43211

struct summary {
    int match;
    int err;
};

struct client_args {
    struct sockaddr_in server_addr;
    int thread_id;
};

// this function is the same as match pattern but returns a boolean instead
int does_match(const unsigned char *buf, int size) {
    unsigned char start = 0xAA;
    for(unsigned int i = 0; i < size; i++){
        if( (0xFFu & buf[i]) != ((start + i) & 0xFFu)){

            printf("wrong pattern here ? returning %s , index %d buf 0x%x patt 0x%x \n", " <_DO_NOT match> ", i, buf[i], ((start + i) & 0xFFu));
            return MATCH_FAILED;
        };
    }
    return MATCH_OK;
}

void* run_client(void* args)
{
    struct summary* sum = calloc(1, sizeof(struct summary));
    sum->match = MATCH_UNDEF;

    struct client_args* cargs = (struct client_args*) args;

    int server_fd = -1, ret = -1, so_far = 0;

    struct sockaddr_in* server_addr = &cargs->server_addr;
    char debug_buffer[INET_ADDRSTRLEN];
    char tx_buffer[TEST_BUF_SIZE];
    char rx_buffer[TEST_BUF_SIZE];
    bzero(tx_buffer, TEST_BUF_SIZE);
    bzero(rx_buffer, TEST_BUF_SIZE);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( 0 > server_fd) {
        TPRINTF("socket creation failed, errno : %d \n", errno);
        sum->err = server_fd;
        goto cleanup;
    }
    TPRINTF("OK: socket created, fd is %d \n", server_fd);

    ret = connect(server_fd, (struct sockaddr*) server_addr, sizeof(struct sockaddr_in));
    if ( 0 != ret) {
        TPRINTF("Error: connection with the server failed, errno %d \n", errno);
        sum->err = ret;
        goto cleanup;
    }
    inet_ntop( AF_INET, &server_addr->sin_addr, debug_buffer, sizeof(debug_buffer));
    TPRINTF("OK: connected to the server at %s \n", debug_buffer);
    // write a pattern
    write_pattern(tx_buffer, TEST_BUF_SIZE);

    // send test buffer
    while (so_far < TEST_BUF_SIZE){
        ret = send(server_fd, tx_buffer + so_far, TEST_BUF_SIZE - so_far, 0);
        if( 0 > ret){
            TPRINTF("Error: send failed with ret %d and errno %d \n", ret, errno);
            sum->err = ret;
            goto cleanup;
        }
        so_far+=ret;
        TPRINTF("\t [send loop] %d bytes, looping again, so_far %d target %d \n", ret, so_far, TEST_BUF_SIZE);
    }
    TPRINTF("OK: buffer sent successfully \n");
    TPRINTF("OK: waiting to receive data \n");
    // receive test buffer
    so_far = 0;
    while (so_far < TEST_BUF_SIZE) {
        ret = recv(server_fd, rx_buffer + so_far, TEST_BUF_SIZE - so_far, 0);
        if( 0 > ret){
            TPRINTF("Error: recv failed with ret %d and errno %d \n", ret, errno);
            sum->err = ret;
            goto cleanup;
        }
        so_far+=ret;
        TPRINTF("\t [receive loop] %d bytes, looping again, so_far %d target %d \n", ret, so_far, TEST_BUF_SIZE);
    }
    TPRINTF("Results of pattern matching: %s \n", match_pattern(rx_buffer, TEST_BUF_SIZE));
    sum->match = does_match(rx_buffer, TEST_BUF_SIZE);

    // close the socket
    // now we sleep a bit to drain the queues and then trigger the close logic
    TPRINTF("A 5 sec wait before calling close \n");
    sleep(5);
    ret = close(server_fd);
    if(ret){
        TPRINTF("Shutdown was not clean , ret %d errno %d \n ", ret, errno);
        sum->err = ret;
        goto cleanup;
    }
    TPRINTF("OK: shutdown was fine. Good bye!\n");
    sum->err = 0;

cleanup:
    free(args);
    return (void*) sum;
}

int main(int argc, char** argv){
    int ret = -1;
    struct sockaddr_in base_addr;
    in_port_t base_port;
    
    bzero(&base_addr, sizeof(base_addr));
    base_addr.sin_family = AF_INET;
    int threads_count = 0;

    if(argc == 4) {
        printf("Setting up the IP: %s, initial port %d with %d threads \n", argv[1], atoi(argv[2]), atoi(argv[3]));
        ret = get_addr(argv[1], (struct sockaddr*) &base_addr);
        if (ret) {
            printf("Invalid IP %s \n", argv[1]);
            return ret;
        }
        base_port = strtol(argv[2], NULL, 0);
        threads_count = atoi(argv[3]);
    } else {
        printf("Wrong number of arguments\n");
        return 1;
    }

    pthread_t threads[threads_count];

    for(int i = 0; i < threads_count; i++){
        struct client_args* cargs = calloc(1, sizeof(struct client_args));
        memcpy(&cargs->server_addr, &base_addr, sizeof(struct sockaddr_in));
        cargs->server_addr.sin_port = htons(base_port + i);
        cargs->thread_id = i;
        
        pthread_create(&threads[i], NULL, run_client, (void*)cargs);
    }

    struct summary* sums[threads_count];

    for(int i = 0; i < threads_count; i++) {
        pthread_join(threads[i], (void**) &sums[i]);
    }

    printf("\nAll finished\n\n");

    for(int i = 0; i < threads_count; i++) {
        struct summary* sum = sums[i];
        printf("Thread %d returned with error %d and match status %d\n", i, sum->err, sum->match);
        free(sum);
    }
}