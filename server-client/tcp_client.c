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

#include "common.h"

// sudo tcpdump -i wlp2s0 tcp port 43211
int main(int argc, char** argv)
{
    int server_fd = -1, ret = -1, so_far = 0;
    char *active_ip = "127.0.0.1";
    int active_port = PORT;

    struct sockaddr_in server_addr;
    char debug_buffer[INET_ADDRSTRLEN];
    char tx_buffer[TEST_BUF_SIZE];
    char rx_buffer[TEST_BUF_SIZE];
    bzero(tx_buffer, TEST_BUF_SIZE);
    bzero(rx_buffer, TEST_BUF_SIZE);

    printf("usage: ./anp_client ip [default: 127.0.0.1] port [default: %d]\n", PORT);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( 0 > server_fd) {
        printf("socket creation failed, errno : %d \n", errno);
        return -errno;
    }
    printf("OK: socket created, fd is %d \n", server_fd);
    bzero(&server_addr, sizeof(server_addr));
    // assign IP, PORT
    server_addr.sin_family = AF_INET;
    if(argc == 3){
        printf("setting up the IP: %s and port %d \n", argv[1], atoi(argv[2]));
        active_ip = argv[1];
        active_port = atoi(argv[2]);
    } else if (argc == 2){
        printf("setting up the IP: %s and port %d \n", argv[1], PORT);
        active_ip = argv[1];
        active_port = PORT;
    } else {
        printf("default IP: 127.0.0.1 and port %d \n", PORT);
        active_ip = "127.0.0.1";
        active_port = PORT;
    }

    ret = get_addr(active_ip, (struct sockaddr*) &server_addr);
    if( ret != 0) {
        printf("Error: Invalid IP %s \n",active_ip);
        return ret;
    }
    server_addr.sin_port = htons(active_port);

    ret = connect(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if ( 0 != ret) {
        printf("Error: connection with the server failed, errno %d \n", errno);
        return errno;
    }
    inet_ntop( AF_INET, &server_addr.sin_addr, debug_buffer, sizeof(debug_buffer));
    printf("OK: connected to the server at %s \n", debug_buffer);

    // write a pattern
    write_pattern(tx_buffer, TEST_BUF_SIZE);

    // send test buffer
    while (so_far < TEST_BUF_SIZE){
        ret = send(server_fd, tx_buffer + so_far, TEST_BUF_SIZE - so_far, 0);
        if( 0 > ret){
            printf("Error: send failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far+=ret;
        printf("\t [send loop] %d bytes, looping again, so_far %d target %d \n", ret, so_far, TEST_BUF_SIZE);
    }

    printf("OK: buffer sent successfully \n");
    printf("OK: waiting to receive data \n");
    // receive test buffer
    so_far = 0;
    while (so_far < TEST_BUF_SIZE) {
        ret = recv(server_fd, rx_buffer + so_far, TEST_BUF_SIZE - so_far, 0);
        if( 0 > ret){
            printf("Error: recv failed with ret %d and errno %d \n", ret, errno);
            return -ret;
        }
        so_far+=ret;
        printf("\t [receive loop] %d bytes, looping again, so_far %d target %d \n", ret, so_far, TEST_BUF_SIZE);
    }
    printf("Results of pattern matching: %s \n", match_pattern(rx_buffer, TEST_BUF_SIZE));
    // close the socket
    // now we sleep a bit to drain the queues and then trigger the close logic
    printf("A 5 sec wait before calling close \n");
    sleep(5);
    ret = close(server_fd);
    if(ret){
        printf("Shutdown was not clean , ret %d errno %d \n ", ret, errno);
        return -ret;
    }
    printf("OK: shutdown was fine. Good bye!\n");
    return 0;
}