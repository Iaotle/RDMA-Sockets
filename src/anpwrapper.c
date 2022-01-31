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

//XXX: _GNU_SOURCE must be defined before including dlfcn to get RTLD_NEXT symbols
#define _GNU_SOURCE

#include <dlfcn.h>
#include "systems_headers.h"
#include "linklist.h"
#include "anpwrapper.h"
#include "init.h"
#include "config.h"
#include "utilities.h"
#include "errno.h"
#include "tcp/tcb_store.h"
#include "tcp/tcp.h"
#include "tcp/ringbuffer.h"

static int (*__start_main)(int (*main) (int, char * *, char * *), int argc, \
                           char * * ubp_av, void (*init) (void), void (*fini) (void), \
                           void (*rtld_fini) (void), void (* stack_end));

static ssize_t (*_send)(int fd, const void *buf, size_t n, int flags) = NULL;
static ssize_t (*_recv)(int fd, void *buf, size_t n, int flags) = NULL;

static int (*_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen) = NULL;
static int (*_socket)(int domain, int type, int protocol) = NULL;
static int (*_close)(int sockfd) = NULL;

static int is_socket_supported(int domain, int type, int protocol)
{
    if (domain != AF_INET){
        return 0;
    }
    if (!(type & SOCK_STREAM)) {
        return 0;
    }
    if (protocol != 0 && protocol != IPPROTO_TCP) {
        return 0;
    }
    printf("supported socket domain %d type %d and protocol %d \n", domain, type, protocol);
    return 1;
}

int socket(int domain, int type, int protocol) {
    if (is_socket_supported(domain, type, protocol)) {
        uint16_t tcb_idx = new_tcb_idx();
        if(tcb_idx < 0){
            return -ENOBUFS;
        }
        if(init_tcb_idx(tcb_idx) < 0){
            return -EACCES;
        }
        return tcb_idx + ANP_FD_OFFSET;
    }
    return _socket(domain, type, protocol);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{    
    int tcb_idx = sockfd - ANP_FD_OFFSET;
    if(!is_valid_tcb(tcb_idx)){
        return _connect(sockfd, addr, addrlen);
    }

    struct sockaddr_in* sockaddr_in = (struct sockaddr_in*) addr;
    uint32_t daddr = ntohl(sockaddr_in->sin_addr.s_addr);
    uint16_t dport = ntohs(sockaddr_in->sin_port);
    uint32_t saddr = ip_str_to_h32(ANP_IP_CLIENT_EXT);

    struct tcb* tcb = get_tcb_at_idx(tcb_idx, true);
    int err = set_tcb_conn(tcb, saddr, daddr, sockfd, dport);
    if (err < 0){
        unlock_tcb_at_idx(tcb_idx);
        errno = EISCONN;
        return -1;
    }

	tcp_send_syn(tcb);

    while (tcb && (tcb->state == TCP_STATE_SYN_SENT || tcb->state == TCP_STATE_SYN_RECEIVED)){
        wait_for_tcb_change(tcb_idx);
        tcb = get_tcb_at_idx(tcb_idx, false);
    }

    if (!tcb || tcb->state != TCP_STATE_ESTABLISHED){
        unlock_tcb_at_idx(tcb_idx);
        errno = ETIMEDOUT;
        return -1;
    }

    unlock_tcb_at_idx(tcb_idx);
    return 0;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    int tcb_idx = sockfd - ANP_FD_OFFSET;
    if(!is_valid_tcb(tcb_idx)){
        return _send(sockfd, buf, len, flags);
    }

    struct tcb* tcb = get_tcb_at_idx(tcb_idx, true);
    if(!tcb || tcb->state != TCP_STATE_ESTABLISHED){
        unlock_tcb_at_idx(tcb_idx);
        errno = ENOTCONN;
        return -1;
    }
    size_t send_len = MIN(len, MIN(tcb->remote_wnd, TCP_OVER_ETH_MAX_PAYLOAD_SIZE));
    
    uint32_t tcp_pkt_size = TCP_PACKET_SIZE(send_len);
    struct subuff* sub = alloc_tcp_sub(tcp_pkt_size, tcb_idx, send_len, TCP_RETRANSMIT_TTL);
	sub_reserve(sub, tcp_pkt_size);
    sub_push(sub, send_len);
    memcpy(sub->data, (uint8_t*) buf, send_len);

    send_tcp_packet(sub, tcb, TCP_ACK);
    unlock_tcb_at_idx(tcb_idx);
    return send_len;
}

ssize_t recv (int sockfd, void *buf, size_t len, int flags){
    int tcb_idx = sockfd - ANP_FD_OFFSET;
    if(!is_valid_tcb(tcb_idx)){
        return _recv(sockfd, buf, len, flags);
    }

    struct tcb* tcb = get_tcb_at_idx(tcb_idx, true);
    while (tcb && tcb->state == TCP_STATE_ESTABLISHED && ringbuffer_empty(tcb->rx_buff)){
        wait_for_tcb_change(tcb_idx);
        tcb = get_tcb_at_idx(tcb_idx, false);
    }

    if(!tcb || tcb->state != TCP_STATE_ESTABLISHED){
        unlock_tcb_at_idx(tcb_idx);
        errno = ENOTCONN;
        return -1;
    }
    bool rx_buf_is_full = ringbuffer_unused_elem_count(tcb->rx_buff) == 0;
    int ret = ringbuffer_remove(tcb->rx_buff, (int8_t*)buf, len);

    // Send ACK to notify sender that the window size has updated if the buffer was full
    if(rx_buf_is_full){
        tcp_send_ack(tcb);
    }

    unlock_tcb_at_idx(tcb_idx);
    return ret;
}

int close (int sockfd){
    int tcb_idx = sockfd - ANP_FD_OFFSET;
    if(!is_valid_tcb(tcb_idx)){
        return _close(sockfd);
    }

    struct tcb* tcb = get_tcb_at_idx(tcb_idx, true);
    if(tcb->state != TCP_STATE_ESTABLISHED){
        unlock_tcb_at_idx(tcb_idx);
        errno = ENOTCONN;
        return -1;
    }

    tcp_send_fin(tcb);
    while (tcb){
        wait_for_tcb_change(tcb_idx);
        tcb = get_tcb_at_idx(tcb_idx, false);
    }
    
    unlock_tcb_at_idx(tcb_idx);
    return 0;
}

void _function_override_init()
{
    __start_main = dlsym(RTLD_NEXT, "__libc_start_main");
    _socket = dlsym(RTLD_NEXT, "socket");
    _connect = dlsym(RTLD_NEXT, "connect");
    _send = dlsym(RTLD_NEXT, "send");
    _recv = dlsym(RTLD_NEXT, "recv");
    _close = dlsym(RTLD_NEXT, "close");
}
