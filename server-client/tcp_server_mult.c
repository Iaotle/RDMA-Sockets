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

#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "common.h"

#include <pthread.h>

#define MATCH_OK 0
#define MATCH_FAILED 1
#define MATCH_UNDEF 2

#define TPRINTF(str, ...) printf("[Thread %d] " str, sargs->thread_id, ##__VA_ARGS__);

struct summary {
    int match;
    int err;
};

struct server_args {
    struct sockaddr_in server_addr;
    int thread_id;
};

void* worker(void* args);

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

int main(int argc, char** argv) {
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

    for (int i = 0; i < threads_count; i++) {
        struct server_args* sargs = calloc(1, sizeof(struct server_args));
        memcpy(&sargs->server_addr, &base_addr, sizeof(struct sockaddr_in));
        sargs->server_addr.sin_port = htons(base_port + i);
        sargs->thread_id = i;
        pthread_create(&threads[i], NULL, worker, sargs);
        printf("Thread %d: Listening on port %d \n", i, base_port + i);
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

void* worker(void* args) {
    struct summary* sum = calloc(1, sizeof(struct summary));
    sum->match = MATCH_UNDEF;

    struct server_args* sargs = (struct server_args*) args;

    int listen_fd, client_fd, len = 0, ret = -1, so_far = 0;
    int optval = 1;
    struct sockaddr_in *server_addr, client_addr;
    char debug_buffer[INET_ADDRSTRLEN];
    char test_buffer[TEST_BUF_SIZE];

    // create the listening socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( -1 == listen_fd) {
        TPRINTF("Error: listen socket failed, ret %d and errno %d \n", listen_fd, errno);
        sum->err = -errno;
        goto cleanup;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    server_addr = (struct sockaddr_in*) &sargs->server_addr;

    //server_addr.sin_family = AF_INET;
    //server_addr.sin_port = htons(conn->port);
    //server_addr.sin_addr.s_addr = htons(conn->addr);

    inet_ntop( AF_INET, &server_addr->sin_addr, debug_buffer, sizeof(debug_buffer));
    //printf("OK: going to bind at %s \n", debug_buffer);
    bzero(debug_buffer, INET_ADDRSTRLEN);

    // bind the socket
    ret = bind(listen_fd, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in));
    if (0 != ret) {
        TPRINTF("Error: socket bind failed, ret %d, errno %d \n", ret, errno);
        sum->err = -errno;
        goto cleanup;
    }
    
    // listen on the socket
    ret = listen(listen_fd, 1); // only 1 backlog
    if (0 != ret) {
        TPRINTF("Error: listen failed ret %d and errno %d \n", ret, errno);
        sum->err = -errno;
        goto cleanup;
    }

    len = sizeof(client_addr);
    client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);
    if ( 0 > client_fd) {
        TPRINTF("Error: accept failed ret %d errno %d \n", client_fd, errno);
        sum->err = -errno;
        goto cleanup;
    }

    inet_ntop( AF_INET, &client_addr.sin_addr, debug_buffer, sizeof(debug_buffer));
    TPRINTF("new incoming connection from %s \n", debug_buffer);
    // first recv the buffer, then tx it back as it is
    so_far = 0;
    while (so_far < TEST_BUF_SIZE) {
        TPRINTF("entered loop \n");
        ret = recv(client_fd, test_buffer + so_far, TEST_BUF_SIZE - so_far, 0);
        TPRINTF("received something \n");
        if( 0 > ret){
            TPRINTF("Error: recv failed with ret %d and errno %d \n", ret, errno);
            sum->err = -ret;
            goto cleanup;
        }
        so_far+=ret;
        TPRINTF("\t [receive loop] %d bytes, looping again, so_far %d target %d \n", ret, so_far, TEST_BUF_SIZE);
    }
    TPRINTF("OK: buffer received ok, pattern match : %s  \n", match_pattern(test_buffer, TEST_BUF_SIZE));
    sum->match = does_match(test_buffer, TEST_BUF_SIZE);
    // then tx it back as it is
    so_far = 0;
    while (so_far < TEST_BUF_SIZE){
        ret = send(client_fd, test_buffer + so_far, TEST_BUF_SIZE - so_far, 0);
        if( 0 > ret){
            TPRINTF("Error: send failed with ret %d and errno %d \n", ret, errno);
            sum->err = -ret;
            goto cleanup;
        }
        so_far+=ret;
        TPRINTF("\t [send loop] %d bytes, looping again, so_far %d target %d \n", ret, so_far, TEST_BUF_SIZE);
    }
    TPRINTF("OK: buffer tx backed \n");

    // in order to initiate the connection close from the client side, we wait here indefinitely to receive more
    ret = recv(client_fd, test_buffer, TEST_BUF_SIZE, 0);
    TPRINTF("ret from the recv is %d errno %d \n", ret, errno);

    // close the two fds
    ret = close(client_fd);
    if(ret){
        TPRINTF("Error: client shutdown was not clean , ret %d errno %d \n ", ret, errno);
        sum->err = -ret;
        goto cleanup;
    }
    ret = close(listen_fd);
    if(ret){
        TPRINTF("Error: server listen shutdown was not clean , ret %d errno %d \n ", ret, errno);
        sum->err = -ret;
        goto cleanup;
    }
    TPRINTF("OK: server and client sockets closed\n");
    sum->err = 0;

cleanup:
    free(args);
    return (void*) sum;
}
