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

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../src/timer.h"
#include "common.h"

int main(int argc, char** argv) {
    int listen_fd, client_fd, len = 0, ret = -1;
    int optval = 1;
    struct sockaddr_in server_addr, client_addr;
    char debug_buffer[INET_ADDRSTRLEN];

    // create the listening socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listen_fd) {
        printf("Error: listen socket failed, ret %d and errno %d \n", listen_fd, errno);
        return (-errno);
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    printf("Socket successfully created, fd = %d \n", listen_fd);
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    // XXX: get_addr overwrites the whole structure, so only set the port after calling this function
    if (argc == 3) {
        printf("setting up the IP: %s and port %d (both) \n", argv[1], atoi(argv[2]));
        ret = get_addr(argv[1], (struct sockaddr*)&server_addr);
        if (ret) {
            printf("Invalid IP %s \n", argv[1]);
            return ret;
        }
        server_addr.sin_port = htons(strtol(argv[2], NULL, 0));
    } else if (argc == 2) {
        printf("setting up the IP: %s and port %d (only IP) \n", argv[1], PORT);
        ret = get_addr(argv[1], (struct sockaddr*)&server_addr);
        if (ret) {
            printf("Invalid IP %s \n", argv[1]);
            return ret;
        }
        server_addr.sin_port = htons(PORT);

    } else {
        printf("default IP: 0.0.0.0 and port %d (none) \n", PORT);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(PORT);
    }

    inet_ntop(AF_INET, &server_addr.sin_addr, debug_buffer, sizeof(debug_buffer));
    printf("OK: going to bind at %s \n", debug_buffer);
    bzero(debug_buffer, INET_ADDRSTRLEN);

    // bind the socket
    ret = bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (0 != ret) {
        printf("Error: socket bind failed, ret %d, errno %d \n", ret, errno);
        exit(-errno);
    }
    printf("Socket successfully binded\n");
    // listen on the socket
    ret = listen(listen_fd, 1);  // only 1 backlog
    if (0 != ret) {
        printf("Error: listen failed ret %d and errno %d \n", ret, errno);
        exit(-errno);
    }

    printf("Server listening.\n");

    len = sizeof(client_addr);
    client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);
    if (0 > client_fd) {
        printf("Error: accept failed ret %d errno %d \n", client_fd, errno);
        exit(-errno);
    }

    inet_ntop(AF_INET, &client_addr.sin_addr, debug_buffer, sizeof(debug_buffer));
    printf("new incoming connection from %s \n", debug_buffer);

    // SaNITY
    printf(ANSI_COLOR_RED "RUNNING SaNITY CHECK\n" ANSI_COLOR_RESET);
    char tx_buffer[TEST_BUFFER_LENGTH];
    write_pattern(tx_buffer, TEST_BUFFER_LENGTH);
    send(client_fd, tx_buffer, TEST_BUFFER_LENGTH, 0);  // SEND

    char rx_buffer[TEST_BUFFER_LENGTH];
    bzero(rx_buffer, TEST_BUFFER_LENGTH);
    int so_far = 0;
    while (so_far < TEST_BUFFER_LENGTH) {
        int ret = recv(client_fd, rx_buffer + so_far, TEST_BUFFER_LENGTH - so_far, 0);  // RECV
        // printf("recv\n");
        if (0 > ret) {
            printf("Error: recv failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far += ret;
    }
    int count = 0;
    while (match_pattern2(rx_buffer, TEST_BUFFER_LENGTH) && count <= 10) {
        count++;
    };
    if (count == 11) {
        printf(ANSI_COLOR_RED "SANITY CHECK FAILED\n" ANSI_COLOR_RESET);
        return -1;
    }

    printf(ANSI_COLOR_GREEN "SaNITY CHECK OK\n" ANSI_COLOR_RESET);











    printf(ANSI_COLOR_YELLOW "RUNNING SEND TEST:\n" ANSI_COLOR_RESET);
    send_test(client_fd);
    printf(ANSI_COLOR_YELLOW "RUNNING RECEIVE TEST:\n" ANSI_COLOR_RESET);
    recv_test(client_fd);
    printf(ANSI_COLOR_YELLOW "RUNNING SEND TEST:\n" ANSI_COLOR_RESET);
    send_test(client_fd);












    printf(ANSI_COLOR_RED "RUNNING SaNITY CHECK\n" ANSI_COLOR_RESET);
    char tx_buffer2[TEST_BUFFER_LENGTH];
    write_pattern2(tx_buffer2, TEST_BUFFER_LENGTH);
    send(client_fd, tx_buffer2, TEST_BUFFER_LENGTH, 0);  // SEND

    char rx_buffer2[TEST_BUFFER_LENGTH];
    bzero(rx_buffer2, TEST_BUFFER_LENGTH);
    so_far = 0;
    while (so_far < TEST_BUFFER_LENGTH) {
        int ret = recv(client_fd, rx_buffer2 + so_far, TEST_BUFFER_LENGTH - so_far, 0);  // RECV
        // printf("recv\n");
        if (0 > ret) {
            printf("Error: recv failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far += ret;
    }
    count = 0;
    while (match_pattern(rx_buffer2, TEST_BUFFER_LENGTH) && count <= 10) {
        count++;
    };
    if (count == 11) {
        printf(ANSI_COLOR_RED "SANITY CHECK FAILED\n" ANSI_COLOR_RESET);
        return -1;
    }

    printf(ANSI_COLOR_GREEN "SaNITY CHECK OK\n" ANSI_COLOR_RESET);








    // close the two fds
    ret = close(client_fd);
    if (ret) {
        printf("Error: client shutdown was not clean , ret %d errno %d \n ", ret, errno);
        return -ret;
    }
    ret = close(listen_fd);
    if (ret) {
        printf("Error: server listen shutdown was not clean , ret %d errno %d \n ", ret, errno);
        return -ret;
    }
    printf("OK: server and client sockets closed\n");
    return 0;
}
