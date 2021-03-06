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

#define _GNU_SOURCE

#include "rdmawrapper.h"

#include <dlfcn.h>
#include <time.h>

#include "init.h"
#include "rdma_common.h"
#include "socklist.h"
#include "systems_headers.h"

static int (*__start_main)(int (*main)(int, char **, char **), int argc, char **ubp_av, void (*init)(void), void (*fini)(void),
                           void (*rtld_fini)(void), void(*stack_end));

static int (*_send)(int fd, const void *buf, size_t n, int flags) = NULL;

static int (*_recv)(int fd, void *buf, size_t n, int flags) = NULL;

static int (*_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen) = NULL;

static int (*_socket)(int domain, int type, int protocol) = NULL;

static int (*_close)(int sockfd) = NULL;

static int (*_accept)(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len) = NULL;

static int (*_bind)(int socket, const struct sockaddr *server_sockaddr, socklen_t address_len) = NULL;

static int (*_listen)(int fd, int __n) = NULL;
static int (*_setsockopt)(int fd, int level, int optname, const void *optval, socklen_t optlen) = NULL;
static int (*_getsockopt)(int fd, int level, int optname, void *__restrict__ optval, socklen_t *__restrict__ optlen) = NULL;
static int (*_select)(int nfds, fd_set *__restrict__ readfds, fd_set *__restrict__ writefds, fd_set *__restrict__ exceptfds,
                      struct timeval *__restrict__ __timeout) = NULL;

static ssize_t (*_read)(int fd, void *buf, size_t nbytes) = NULL;
// static ssize_t (*_write)(int fd, void *buf, size_t nbytes) = NULL;

ssize_t read(int fd, void *buf, size_t nbytes) {
    debug(ANSI_COLOR_YELLOW "READ fd: %d, buf: %d, nbytes: %d\n" ANSI_COLOR_RESET, fd, buf, nbytes);
    // check against our stored fds, if we don't have it let it go, otherwise use our recv
    if (isEmpty()) {
        debug("no sockets, returning\n");
        return _read(fd, buf, nbytes);
    }
    sock *socket = find(fd);
    if (socket) {
        return recv(fd, buf, nbytes, 0);
    }
    debug("no fds match, returning\n");
    return _read(fd, buf, nbytes);
}

static int is_socket_supported(int domain, int type, int protocol) {
    return 1;
    // if (domain != AF_INET) {
    //     return 0;
    // }
    // if (!(type & SOCK_STREAM)) {
    //     return 0;
    // }
    // if (protocol != 0 && protocol != IPPROTO_TCP) {
    //     return 0;
    // }
    // debug("supported socket domain %d type %d and protocol %d \n", domain, type, protocol);
    // return 1;
}

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) { return 0; }

int getsockopt(int fd, int level, int optname, void *__restrict__ optval, socklen_t *__restrict__ optlen) { return 0; }

int select(int nfds, fd_set *__restrict__ readfds, fd_set *__restrict__ writefds, fd_set *__restrict__ exceptfds,
           struct timeval *__restrict__ __timeout) {
    debug(ANSI_COLOR_YELLOW "\tSELECT CALL: %d\n" ANSI_COLOR_RESET, nfds);
    if (!isEmpty()) return 1;
    return -1;
}

int socket(int domain, int type, int protocol) {
    debug(ANSI_COLOR_YELLOW "\tSOCKET CALL\n" ANSI_COLOR_RESET);

    if (is_socket_supported(domain, type, protocol)) {
        int ret = -1;
        sock *socket = (sock *)malloc(sizeof(sock));
        if (!socket) {
            debug("Failed to create socket\n");
            return -1;
        }
        if (!create(socket)) {
            debug("Failed to create node\n");
            return -1;
        }

        // int num = 0;
        // struct ibv_context **context = rdma_get_devices(&num);
        // for (int i = 0; i < num; i++) {
        //     struct ibv_device_attr attr;
        //     ibv_query_device(context[i], &attr);
        //     dprintf(STDOUT_FILENO, "\n");
        // }

        /*  Open a channel used to report asynchronous communication event */
        socket->event_channel = rdma_create_event_channel();

        if (!socket->event_channel) {
            rdma_error("Creating cm event channel failed with errno : (%d)", -errno);
            return -errno;
        }
        debug("RDMA CM event channel is created successfully at %p \n", socket->event_channel);
        /* rdma_cm_id is the connection identifier (like socket) which is used
         * to define an RDMA connection.
         */
        ret = rdma_create_id(socket->event_channel, &socket->cm_id, NULL, RDMA_PS_TCP);
        debug(ANSI_COLOR_YELLOW "\tRDMA ID created\n" ANSI_COLOR_RESET);
        if (ret) {
            rdma_error("Creating cm id failed with errno: %d ", -errno);
            return -errno;
        }
        socket->gc_counter = 0;
        socket->local_written = 0;
        socket->remote_written = 0;

        debug("socket made with id %d\n", socket->cm_id);
        socket->fd = (intptr_t)socket->cm_id;

        return (intptr_t)socket->cm_id;
    }
    return _socket(domain, type, protocol);
}

int listen(int fd, int n) {
    debug(ANSI_COLOR_YELLOW "\tLISTEN CALL\n" ANSI_COLOR_RESET);

    bool is_rdma_sockfd = false;
    sock *sock = find(fd);
    if (sock) {
        debug("listen with fd %d\n", fd);
        /* Now we start to listen on the passed IP and port. However unlike
         * normal TCP listen, this is a non-blocking call. When a new client is
         * connected, a new connection management (CM) event is generated on the
         * RDMA CM event channel from where the listening id was created. Here we
         * have only one channel, so it is easy. */
        int ret = rdma_listen(sock->cm_id, 8); /* backlog = 8 clients, same as TCP, see man listen*/
        if (ret) {
            rdma_error("rdma_listen failed to listen on server address, errno: %d ", -errno);
            return -errno;
        }

        return ret;
    }
    return _listen(fd, n);
}

/**
 * @brief Exchange receive buffer metadata.
 * @details
 * 1. Allocate a receive buffer
 * 2. Pre-post a receive buffer for metadata of the receive buffer of the remote
 * 3. Post buffer address to the remote
 * 4. Get remote buffer from pre-posted receive buffer.
 * @param sock
 * @return NULL on error, 0 on successful metadata exchange
 */
int exchange_metadata(sock *sock) {
    if (!sock) {
        debug("error: no socket provided to exchange metadata\n");
        return NULL;
    }
    if (!sock->pd) {
        debug("error: no socket pd\n");
        return NULL;
    }

    if (!sock->local_buf) {  // 1. allocate a receive buffer
        sock->local_buf = rdma_buffer_alloc(sock->pd /* which protection domain */, RECVBUF_SIZE /* what size to allocate */,
                                            (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE), /* access permissions */
                                            NULL);                                              // will need calloc here.
    }

    if (!sock->send_buf) {  // 1. allocate a send buffer
        sock->send_buf = rdma_buffer_alloc(sock->pd /* which protection domain */, SNDBUF_SIZE /* what size to allocate */,
                                           (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE), /* access permissions */
                                           NULL);                                              // will need calloc here.
    }

    // 2. pre-post a receive buffer for remote metadata
    struct ibv_sge send_sge;
    struct ibv_recv_wr recv_wr, *bad_recv_wr;
    struct ibv_mr *receive_mr = rdma_buffer_register(sock->pd, &sock->remote_buf, sizeof(sock->remote_buf), (IBV_ACCESS_LOCAL_WRITE));
    if (!receive_mr) {
        rdma_error("Failed to setup the metadata mr , -ENOMEM\n");
        return -ENOMEM;
    }
    send_sge.addr = (uint64_t)receive_mr->addr;
    send_sge.length = (uint32_t)receive_mr->length;
    send_sge.lkey = (uint32_t)receive_mr->lkey;
    /* now we link it to the request */
    bzero(&recv_wr, sizeof(recv_wr));
    recv_wr.sg_list = &send_sge;
    recv_wr.num_sge = 1;

    int ret = ibv_post_recv(sock->qp /* which QP */, &recv_wr /* receive work request*/, &bad_recv_wr /* error WRs */);
    if (ret) {
        rdma_error("Failed to post the receive buffer, errno: %d \n", ret);
        return ret;
    }
    debug("Receive buffer posting is successful \n");

    // 3. post buffer to remote
    struct rdma_buffer_attr buffer_attr;
    struct ibv_mr *metadata_mr = rdma_buffer_register(sock->pd, &buffer_attr, sizeof(buffer_attr), (IBV_ACCESS_LOCAL_WRITE));
    struct ibv_send_wr send_wr, *bad_send_wr;

    if (!metadata_mr) {
        rdma_error("Failed to register the metadata buffer, ret = %d \n", ret);
        return ret;
    }

    /* we prepare metadata for the data buffer */
    buffer_attr.address = (uint64_t)sock->local_buf->addr;
    buffer_attr.length = sock->local_buf->length;
    buffer_attr.stag.local_stag = sock->local_buf->lkey;

    /* now we fill up SGE */
    send_sge.addr = (uint64_t)metadata_mr->addr;
    send_sge.length = (uint32_t)metadata_mr->length;
    send_sge.lkey = metadata_mr->lkey;
    /* now we link to the send work request */
    bzero(&send_wr, sizeof(send_wr));
    send_wr.sg_list = &send_sge;
    send_wr.num_sge = 1;
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    /* Now we post it */
    debug("send: posting our metadata to send\n");

    ret = ibv_post_send(sock->qp, &send_wr, &bad_send_wr);
    if (ret) {
        rdma_error("Failed to send client metadata, errno: %d \n", -errno);
        return -errno;
    }

    struct ibv_wc wc[2];
    /* at this point we are expecting 2 work completions. One for our
     * send and one for recv that we will get from the remote for
     * its buffer information */
    ret = process_work_completion_events(sock->completion_channel, wc, 2);
    if (ret != 2) {
        rdma_error("We failed to get 2 work completions , ret = %d \n", ret);
        return ret;
    }
    debug("Credentials of the remote recvbuf:\n");
    show_rdma_buffer_attr(&sock->remote_buf);

    // debug("prepost receive buffer\n");
    // ret = ibv_post_recv(sock->qp /* which QP */, &recv_wr /* receive work request*/, &bad_recv_wr /* error WRs */);
    // if (ret) {
    //     rdma_error("Failed to post the receive buffer, errno: %d \n", ret);
    //     return ret;
    // }

    return 0;
}

int accept(int fd, struct sockaddr *restrict address, socklen_t *restrict address_len) {
    // setup client resources
    debug(ANSI_COLOR_YELLOW "\tACCEPT CALL\n" ANSI_COLOR_RESET);

    bool is_rdma_sockfd = false;
    sock *sock = find(fd);
    if (sock) {
        /* now, we expect a client to connect and generate a
         * RDMA_CM_EVENT_CONNECT_REQUEST We wait (block) on the connection
         * management event channel for the connect event.
         */
        int ret = -1;
        struct rdma_cm_event *cm_event = NULL;
        ret = process_rdma_cm_event(sock->event_channel, RDMA_CM_EVENT_CONNECT_REQUEST, &cm_event);
        if (ret) {
            rdma_error("Failed to get cm event, ret = %d \n", ret);
            return ret;
        }
        /* Much like TCP connection, listening returns a new connection identifier
         * for newly connected client. In the case of RDMA, this is stored in id
         * field. For more details: man rdma_get_cm_event
         */

        struct sock *new_socket = (struct sock *)malloc(sizeof(struct sock));
        if (!new_socket) {
            debug("Failed to create socket\n");
            return -1;
        }
        memcpy(new_socket, sock, sizeof(struct sock));  // inherit data from old socket
        new_socket->cm_id = cm_event->id;
        new_socket->fd = (intptr_t)cm_event->id;
        new_socket->gc_counter = 0;
        new_socket->local_written = 0;
        new_socket->remote_written = 0;
        /* now we acknowledge the event. Acknowledging the event free the resources
         * associated with the event structure. Hence any reference to the event
         * must be made before acknowledgment. Like, we have already saved the
         * client id from "id" field before acknowledging the event.
         */
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
            rdma_error("Failed to acknowledge the cm event errno: %d \n", -errno);
            return -errno;
        }
        debug("A new RDMA client connection id is stored at %p\n", new_socket->cm_id);

#pragma region allocate_structs
        /* We have a valid connection identifier, lets start to allocate
         * resources. We need:
         * 1. Protection Domains (PD)
         * 2. Memory Buffers
         * 3. Completion Queues (CQ)
         * 4. Queue Pair (QP)
         * Protection Domain (PD) is similar to a "process abstraction"
         * in the operating system. All resources are tied to a particular PD.
         * And accessing recourses across PD will result in a protection fault.
         */
        new_socket->pd = ibv_alloc_pd(new_socket->cm_id->verbs
		/* verbs defines a verb's provider,
		* i.e an RDMA device where the incoming
		* client connection came */);
        if (!new_socket->pd) {
            rdma_error("Failed to allocate a protection domain errno: %d\n", -errno);
            return -errno;
        }
        debug("A new protection domain is allocated at %p \n", new_socket->pd);
        /* Now we need a completion channel, were the I/O completion
         * notifications are sent. Remember, this is different from connection
         * management (CM) event notifications.
         * A completion channel is also tied to an RDMA device, hence we will
         * use cm_host_id->verbs.
         */
        new_socket->completion_channel = ibv_create_comp_channel(new_socket->cm_id->verbs);
        if (!new_socket->completion_channel) {
            rdma_error("Failed to create an I/O completion event channel, %d\n", -errno);
            return -errno;
        }
        debug("An I/O completion event channel is created at %p \n", new_socket->completion_channel);
        /* Now we create a completion queue (CQ) where actual I/O
         * completion metadata is placed. The metadata is packed into a structure
         * called struct ibv_wc (wc = work completion). ibv_wc has detailed
         * information about the work completion. An I/O request in RDMA world
         * is called "work" ;)
         */
        new_socket->cq =
            ibv_create_cq(new_socket->cm_id->verbs /* which device*/, CQ_CAPACITY /* maximum capacity*/, NULL /* user context, not used here */,
                          new_socket->completion_channel /* which IO completion channel */, 0 /* signaling vector, not used here*/);
        if (!new_socket->cq) {
            rdma_error("Failed to create a completion queue (cq), errno: %d\n", -errno);
            return -errno;
        }
        debug("Completion queue (CQ) is created at %p with %d elements \n", new_socket->cq, new_socket->cq->cqe);
        /* Ask for the event for all activities in the completion queue*/
        ret = ibv_req_notify_cq(new_socket->cq /* on which CQ */, 0 /* 0 = all event type, no filter*/);
        if (ret) {
            rdma_error("Failed to request notifications on CQ errno: %d \n", -errno);
            return -errno;
        }
        /* Now the last step, set up the queue pair (send, recv) queues and their
         * capacity. The capacity here is define statically but this can be probed
         * from the device. We just use a small number as defined in rdma_common.h
         */
        struct ibv_qp_init_attr qp_init_attr;
        bzero(&qp_init_attr, sizeof qp_init_attr);
        qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
        qp_init_attr.cap.max_recv_wr = MAX_WR;   /* Maximum receive posting capacity */
        qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
        qp_init_attr.cap.max_send_wr = MAX_WR;   /* Maximum send posting capacity */
        qp_init_attr.qp_type = IBV_QPT_RC;       /* QP type, RC = Reliable connection */
        /* We use same completion queue, but one can use different queues */
        qp_init_attr.recv_cq = new_socket->cq; /* Where should I notify for receive completion operations */
        qp_init_attr.send_cq = new_socket->cq; /* Where should I notify for send completion operations */

        /*Lets create a QP */
        ret = rdma_create_qp(new_socket->cm_id /* which connection id */, new_socket->pd /* which protection domain*/,
                             &qp_init_attr /* Initial attributes */);
        if (ret) {
            rdma_error("Failed to create QP due to errno: %d\n", -errno);
            return -errno;
        }
        /* Save the reference for handy typing but is not required */
        new_socket->qp = new_socket->cm_id->qp;
        debug("Client QP created at %p\n", new_socket->qp);

#pragma endregion

        // accept
        struct rdma_conn_param conn_param;

        memset(&conn_param, 0, sizeof(conn_param));
        /* this tell how many outstanding requests can we handle */
        conn_param.initiator_depth = 7; /* For this exercise, we put a small number here */
        /* This tell how many outstanding requests we expect other side to handle */
        conn_param.responder_resources = 7; /* For this exercise, we put a small number */
        conn_param.rnr_retry_count = 7;
		
        new_socket->event_channel = rdma_create_event_channel();
        ret = rdma_migrate_id(new_socket->cm_id, new_socket->event_channel);
        if (ret) {
            rdma_error("Failed to migrate the connection, errno: %d \n", -errno);
        }

        ret = rdma_accept(new_socket->cm_id, &conn_param);

        if (ret) {
            rdma_error("Failed to accept the connection, errno: %d \n", -errno);
            return -errno;
        }
        /* We expect an RDMA_CM_EVENT_ESTABLISHED to indicate that the RDMA
         * connection has been established and everything is fine on both, server
         * as well as the client sides.
         */
        debug("Going to wait for RDMA_CM_EVENT_ESTABLISHED event \n");

        ret = process_rdma_cm_event(new_socket->event_channel, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
        if (ret) {
            rdma_error("Failed to get the cm event, errnp: %d \n", -errno);
            return -errno;
        }
        debug(ANSI_COLOR_CYAN "accept new id: %d\n" ANSI_COLOR_RESET, new_socket->cm_id);
        /* We acknowledge the event */
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
            rdma_error("Failed to acknowledge the cm event %d\n", -errno);
            return -errno;
        }

        static struct ibv_sge remote_recv_sge;
        static struct ibv_recv_wr remote_recv_wr, *bad_remote_recv_wr = NULL;
        // static struct ibv_mr *client_metadata_mr = NULL;
        new_socket->metadata_mr =
            rdma_buffer_register(new_socket->pd /* which protection domain */, &new_socket->remote_metadata_attr /* what memory */,
                                 sizeof(new_socket->remote_metadata_attr) /* what length */, (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
        if (!&new_socket->remote_metadata_attr) {
            rdma_error("Failed to register client attr buffer\n");
            // we assume ENOMEM
            return -ENOMEM;
        }

        // this will hopefully exchange the receive buffer metadata
        exchange_metadata(new_socket);

        // /* We pre-post this receive buffer for client metadata on the QP. SGE credentials is where we
        //  * receive the metadata from the client */
        // remote_recv_sge.addr = (uint64_t)new_socket->metadata_mr->addr;
        // remote_recv_sge.length = new_socket->metadata_mr->length;
        // remote_recv_sge.lkey = new_socket->metadata_mr->lkey;
        // /* Now we link this SGE to the work request (WR) */
        // bzero(&remote_recv_wr, sizeof(remote_recv_wr));
        // remote_recv_wr.sg_list = &remote_recv_sge;
        // remote_recv_wr.num_sge = 1;  // only one SGE
        // debug("accept: preposting buffer for client metadata to recv\n");
        // ret = ibv_post_recv(new_socket->qp /* which QP */, &remote_recv_wr /* receive work request*/, &bad_remote_recv_wr /* error WRs */);
        // if (ret) {
        //     rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
        //     return ret;
        // }
        // debug("Receive buffer pre-posting is successful \n");

        memcpy(address, rdma_get_peer_addr(new_socket->cm_id), sizeof(struct sockaddr));
        address_len = (int *)sizeof(struct sockaddr);
        debug("A new connection is accepted from %s \n", inet_ntoa(((struct sockaddr_in *)address)->sin_addr));
        if (!create(new_socket)) {
            debug("Failed to create node\n");
            return -1;
        }

        return new_socket->fd;
    }
    return _accept(fd, address, address_len);
}

int bind(int fd, const struct sockaddr *server_sockaddr,
         socklen_t address_len) {  // address_len is useless here because of RDMA

    debug(ANSI_COLOR_YELLOW "\tBIND CALL\n" ANSI_COLOR_RESET);

    bool is_rdma_sockfd = false;
    sock *sock = find(fd);
    if (sock) {
        char *ip = inet_ntoa(((struct sockaddr_in *)server_sockaddr)->sin_addr);
        debug("Trying to bind %s with socket %d\n", ip, sock->cm_id);
        int ret = rdma_bind_addr(sock->cm_id, (struct sockaddr *)server_sockaddr);
        if (ret) {
            rdma_error("Failed to bind server address, errno: %d \n", -errno);
            return -errno;
        }
        return ret;
    }
    return _bind(fd, server_sockaddr, address_len);
}

struct ibv_send_wr send_wrs[1000];
int send_wr_counter = 0;

int num_calloc = 0, post = 0, registration = 0;
double amount_calloc = 0, amount_post = 0, amount_registration = 0;

/// TODO: we can batch the read work completions based on the amount of credits that remain.
int counter = 0;

ssize_t send(int fd, const void *buf, size_t len, int flags) {
    debug(ANSI_COLOR_YELLOW "\tSEND CALL, fd: %d\n" ANSI_COLOR_RESET, fd);

    bool is_rdma_sockfd = false;

    if (isEmpty()) return _send(fd, buf, len, flags);
    sock *sock = find(fd);
    if (sock) {
        // printf("cond: %d, len: %d, size-offset: %d\n", len <= RECVBUF_SIZE - sock->remote_offset, len, RECVBUF_SIZE - sock->remote_offset);
        if (len <= RECVBUF_SIZE - sock->remote_written) {  // can write to recvbuf, and have enough space
            debug(ANSI_COLOR_CYAN "1\n" ANSI_COLOR_RESET);
            // 1. set imm to the size of write
            // 2. rdma_write_imm to the remote buffer
            // 3. wait for completion of write
            // 4. move counter

            int ret = -1;
            /* Step 1: is to copy the local buffer into the remote buffer. We will
             * reuse the previous variables. */
            /* now we fill up SGE */
            struct ibv_sge sge;
            struct ibv_send_wr *send_wr = &send_wrs[send_wr_counter], bad_send_wr;
            // send_wr_counter += 1;
            // if (send_wr_counter == 1000) {
            //     send_wr_counter = 0;
            // }
            struct ibv_wc wc;
            // clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            // timediff = diff(start, end);

            // double time_num = timediff.tv_sec + ((double)timediff.tv_nsec) / 1000000000;
            // if (time_num >= 0.00001) {
            //     num_calloc++;
            //     amount_calloc += time_num;
            //     // printf(ANSI_COLOR_RED "Timediff calloc %f\n" ANSI_COLOR_RESET, time_num);
            // }

            /// register RDMA buffer for the stuff we're sending
            if (!(sock->last_bufptr_send == buf && sock->last_len_send == len)) {  // buffers are the same, no need to re-register
                sock->mr_send = rdma_buffer_register(sock->pd, buf, len, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                IBV_ACCESS_REMOTE_WRITE));
            }
            if (!sock->mr_send) {
                rdma_error("Failed to register the data buffer, ret = %d \n", ret);
                return ret;
            }
            // if (!(sock->last_bufptr_send == buf && sock->last_len_send == len)) {
                // struct timespec timediff;
                // struct timespec start, end;
                // clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                // memcpy(sock->send_buf->addr, buf, len);
                // clock_gettime(CLOCK_MONOTONIC_RAW, &end);
                // timediff = diff(start, end);

                // double time_num = timediff.tv_sec + ((double)timediff.tv_nsec) / 1000000000;
                // if (time_num >= 0.00001) {
                //     registration++;
                //     amount_registration += time_num;
                //     printf(ANSI_COLOR_YELLOW "Timediff memcpy %f\n" ANSI_COLOR_RESET, time_num);
                // }
            // }

            // clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            sge.addr = (uint64_t)sock->mr_send->addr;
            sge.length = (uint32_t)sock->mr_send->length;
            sge.lkey = sock->mr_send->lkey;
            /* now we link to the send work request */
            // bzero(&send_wr, sizeof(send_wr));
            send_wr->sg_list = &sge;
            send_wr->num_sge = 1;
            send_wr->opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
            send_wr->send_flags = IBV_SEND_SIGNALED;
            send_wr->imm_data = len;
            // send the length of the buffer with the immediate call
            /* we have to give info for RDMA */
            send_wr->wr.rdma.rkey = sock->remote_buf.stag.local_stag;
            send_wr->wr.rdma.remote_addr = sock->remote_buf.address + sock->remote_written;
            debug("posting our buffer to send\n");
            ret = ibv_post_send(sock->qp, send_wr, &bad_send_wr);
            /// TODO: IMPORTANT we need to free send_wr or leak memory like bad programmers
            if (ret) {
                rdma_error("Failed to write client source buffer, errno: %d \n", -errno);
                exit(-1);
                return -errno;
            }
            // clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            // timediff = diff(start, end);

            // time_num = timediff.tv_sec + ((double)timediff.tv_nsec) / 1000000000;
            // if (time_num >= 0.00001) {
            //     post++;
            //     amount_post += time_num;
            //     // printf(ANSI_COLOR_MAGENTA "Timediff post send %f\n" ANSI_COLOR_RESET, time_num);
            // }
            // clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            /* at this point we are expecting 1 work completion for the write */

            ret = process_work_completion_events(sock->completion_channel, &wc, 1);
            if (ret != 1) {
                rdma_error("We failed to get %d work completions , ret = %d \n", 1, ret);
                return ret;
            }

            debug("Client side WRITE is complete \n");
            sock->remote_written += len;

            if (!(sock->last_bufptr_send == buf && sock->last_len_send == len)) {  // no need to gc
                sock->gc_container[sock->gc_counter] = sock->mr_send;              // TODO: make this look not as horrible to read maybe?
                sock->gc_counter++;
                gc_sock(sock);
            }
            sock->last_bufptr_send = buf;
            sock->last_len_send = len;

            return len;
        } else if (len <= RECVBUF_SIZE) {  // we need some more space in the receive buffer, but the message itself fits
            debug("1.5\n");

            /// TODO: send a smaller chunk to fill the buffer, then reset everything

            // 1. send the remaining data to fill the buffer completely
            int remaining = RECVBUF_SIZE - sock->remote_written;
            // 2. wait for send to complete

            // 3. set the remote offset to 0, assume we can write to the start of the buffer already
            //    (the buffer should be huge, so this is a non-issue, maybe signal the remote side though)
            sock->remote_written = 0;
            // printf("wut: %d\n", len <= RECVBUF_SIZE - sock->remote_offset);

            debug("buffer filled to the brim, suppose we need some sort of ring buffer confirmation here\n");
            // printf("recursion\n");
            return send(fd, buf, len, flags);

            // 4. return the size we wrote to fill the buffer
            return remaining;  // TODO check for this in server code. we currently just assume we sent the full message
        }

        printf(ANSI_COLOR_RED "2\n" ANSI_COLOR_RESET);

        int ret = -1;
        struct ibv_wc wc[2];
        static struct ibv_recv_wr recv_wr, *bad_recv_wr = NULL;

        /// register RDMA buffer for the stuff we're sending
        if (!(sock->last_bufptr_send == buf && sock->last_len_send == len)) {  // buffers are the same, no need to re-register
            sock->mr_send = rdma_buffer_register(sock->pd, buf, len, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));
        }
        if (!sock->mr_send) {
            rdma_error("Failed to register the data buffer, ret = %d \n", ret);
            return ret;
        }
        struct rdma_buffer_attr local_metadata;
        /* we prepare metadata for the data buffer */
        local_metadata.address = (uint64_t)sock->mr_send->addr;
        local_metadata.length = sock->mr_send->length;
        local_metadata.stag.local_stag = sock->mr_send->lkey;

        static struct ibv_mr *metadata_mr;
        /* now we register the metadata memory */
        metadata_mr = rdma_buffer_register(sock->pd, &local_metadata, sizeof(local_metadata), IBV_ACCESS_LOCAL_WRITE);
        if (!metadata_mr) {
            rdma_error("Failed to register the metadata buffer, ret = %d \n", ret);
            return ret;
        }
        struct ibv_sge send_sge;
        struct ibv_send_wr send_wr, *bad_send_wr;
        /* now we fill up SGE */
        send_sge.addr = (uint64_t)metadata_mr->addr;
        send_sge.length = (uint32_t)metadata_mr->length;
        send_sge.lkey = metadata_mr->lkey;
        /* now we link to the send work request */
        bzero(&send_wr, sizeof(send_wr));  // wow important
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.send_flags = IBV_SEND_SIGNALED;
        /* Now we post it */
        debug("send: posting our metadata to send\n");

        ret = ibv_post_send(sock->qp, &send_wr, &bad_send_wr);
        if (ret) {
            rdma_error("Failed to send client metadata, errno: %d \n", -errno);
            return -errno;
        }
        /* at this point we are expecting 2 work completions. One for our
         * send and one for recv that we will get from the remote for
         * its buffer information */
        ret = process_work_completion_events(sock->completion_channel, wc, 2);
        if (ret != 2) {
            rdma_error("We failed to get 2 work completions , ret = %d \n", ret);
            return ret;
        }
        debug("Remote sent us its buffer location and credentials, showing \n");
        show_rdma_buffer_attr(&sock->remote_metadata_attr);

        rdma_buffer_deregister(metadata_mr);
        // remote write
        ret = -1;
        /* Step 1: is to copy the local buffer into the remote buffer. We will
         * reuse the previous variables. */
        /* now we fill up SGE */
        send_sge.addr = (uint64_t)sock->mr_send->addr;
        send_sge.length = (uint32_t)sock->mr_send->length;
        send_sge.lkey = sock->mr_send->lkey;
        /* now we link to the send work request */
        // bzero(&send_wr, sizeof(send_wr));
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;
        send_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
        send_wr.send_flags = IBV_SEND_SIGNALED;
        send_wr.imm_data = len;  // send the length of the buffer with the immediate call
        /* we have to give info for RDMA */
        send_wr.wr.rdma.rkey = sock->remote_metadata_attr.stag.remote_stag;
        send_wr.wr.rdma.remote_addr = sock->remote_metadata_attr.address;

        debug("posting our buffer to send\n");
        ret = ibv_post_send(sock->qp, &send_wr, &bad_send_wr);
        if (ret) {
            rdma_error("Failed to write client source buffer, errno: %d \n", -errno);
            return -errno;
        }
        /* at this point we are expecting 1 work completion for the write */
        ret = process_work_completion_events(sock->completion_channel, &wc[0], 1);
        if (ret != 1) {
            rdma_error("We failed to get 1 work completions , ret = %d \n", ret);
            return ret;
        }

        debug("Client side WRITE is complete \n");

        rdma_buffer_deregister(sock->metadata_mr);

        sock->metadata_mr = rdma_buffer_register(sock->pd, &sock->remote_metadata_attr, sizeof(sock->remote_metadata_attr), (IBV_ACCESS_LOCAL_WRITE));
        if (!sock->metadata_mr) {
            rdma_error("Failed to setup the server metadata mr , -ENOMEM\n");
            return -ENOMEM;
        }
        send_sge.addr = (uint64_t)sock->metadata_mr->addr;
        send_sge.length = (uint32_t)sock->metadata_mr->length;
        send_sge.lkey = (uint32_t)sock->metadata_mr->lkey;
        /* now we link it to the request */
        bzero(&recv_wr, sizeof(recv_wr));
        recv_wr.sg_list = &send_sge;
        recv_wr.num_sge = 1;
        debug("send: posting a recv request for metadata\n");

        ret = ibv_post_recv(sock->qp /* which QP */, &recv_wr /* receive work request*/, &bad_recv_wr /* error WRs */);
        if (ret) {
            rdma_error("Failed to post the receive buffer, errno: %d \n", ret);
            return ret;
        }
        debug("Receive buffer posting is successful \n");

        if (!(sock->last_bufptr_send == buf && sock->last_len_send == len)) {  // no need to gc
            sock->gc_container[sock->gc_counter] = sock->mr_send;              // TODO: make this look not as horrible to read maybe?
            sock->gc_counter++;
            gc_sock(sock);
        }
        sock->last_bufptr_send = buf;
        sock->last_len_send = len;

        return len;
    }
    return _send(fd, buf, len, flags);
}

ssize_t recv(int fd, void *buf, size_t len, int flags) {
    debug(ANSI_COLOR_YELLOW "\tRECV CALL, fd: %d\n" ANSI_COLOR_RESET, fd);

    bool is_rdma_sockfd = false;

    if (isEmpty()) return _recv(fd, buf, len, flags);

    sock *sock = find(fd);
    if (!sock) {
        debug("no sock\n");
    }

    if (sock) {
        /// TODO: ignore len param for RDMA logic, just process a send request if one is available, otherwise memcpy.
        if (len <= RECVBUF_SIZE - sock->local_written) {  // can read from recvbuf, and have enough space

            // 0. Pre-post receive buffer
            // 1. process write with imm, get length
            // 2. read from local_offset to that position
            // 3. adjust local_offset for next read
            /// 4. TODO: send some sort of ack back to the sender when we have read data, freeing some space on the buffer - basically ring buffer
            /// stuff we've yet to do
            struct ibv_wc wc;
            int ret = -1;
            struct ibv_recv_wr recv_wr, bad_recv_wr;

            bzero(&recv_wr, sizeof(recv_wr));
            debug("prepost receive buffer\n");
            ret = ibv_post_recv(sock->qp /* which QP */, &recv_wr /* receive work request*/, &bad_recv_wr /* error WRs */);
            if (ret) {
                rdma_error("Failed to post the receive buffer, errno: %d \n", ret);
                return ret;
            }

            debug("trying to get IBV_WC_RECV_RDMA_WITH_IMM:\n");
            ret = process_work_completion_events(sock->completion_channel, &wc, 1);
            if (ret != 1) {
                rdma_error("Failed to receive , ret = %d \n", ret);
                return ret;
            }
            debug("success, got %s, local_offset %d\n", msgSize(wc.imm_data), sock->local_written);

            debug("will now copy from  %p to %p\n", sock->local_buf->addr + sock->local_written,
                  sock->local_buf->addr + sock->local_written + wc.imm_data);

            memcpy(buf, sock->local_buf->addr + sock->local_written,
                   wc.imm_data);  /// TODO: track how much data we already read and how much we have available.
            debug("recv_buf_addr: 0x%x\n", *((char *)sock->local_buf->addr));
            sock->local_written += (int)wc.imm_data;  /// TODO: ditto

            if (!(sock->last_bufptr_recv == buf && sock->last_len_recv == len)) {  // we only GC when new buffer is passed
                sock->gc_container[sock->gc_counter] = sock->mr_recv;
                sock->gc_counter++;
                gc_sock(sock);
            }

            sock->last_bufptr_recv = buf;
            sock->last_len_recv = len;

            return len;
        } else if (len <= RECVBUF_SIZE) {  // we need some more space in the receive buffer
            /// TODO: recv a smaller chunk to empty the buffer, then reset everything

            // 1. get the remaining data from the full buffer
            int remaining = RECVBUF_SIZE - sock->local_written;
            // 2. wait for send to complete

            // 3. set the local offset to 0, assume remote can write to the start of the buffer already
            //    (the buffer should be huge, so this is a non-issue, maybe signal the remote side though)
            sock->local_written = 0;
            debug("buffer filled to the brim, suppose we need some sort of ring buffer confirmation here\n");
            return recv(fd, buf, len, flags);

            // 4. return the remaining bit of the buffer
            return remaining;  // TODO check for this in server code. we currently just assume we sent the full message
        }

        int ret = -1;

        static struct ibv_sge client_recv_sge;
        static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
        struct rdma_cm_event *cm_event = NULL;
        // send host metadata to remote
        struct ibv_wc wc;
        ret = -1;
        /* Now, we first wait for the remote to start the communication by
         * sending the host its metadata info. We will receive a work completion
         * notification for our pre-posted receive request.
         */
        ret = process_work_completion_events(sock->completion_channel, &wc, 1);
        if (ret != 1) {
            rdma_error("Failed to receive , ret = %d \n", ret);
            return ret;
        }

        /* if all good, then we should have remote's buffer information, let's
         * see */
        debug("Remote side buffer information is received...\n");
        show_rdma_buffer_attr(&sock->remote_metadata_attr);
        debug("The remote has requested buffer length of : %u bytes \n", sock->remote_metadata_attr.length);
        /* We need to setup requested memory buffer. This is where the sender
         * will do RDMA WRITE. */

        if (!(sock->last_bufptr_recv == buf && sock->last_len_recv == len)) {  // same as last buffer, no need to allocate again
            sock->mr_recv = rdma_buffer_alloc(sock->pd /* which protection domain */, sock->remote_metadata_attr.length /* what size to allocate */,
                                              (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE), /* access permissions */
                                              buf);
        }
        if (!sock->mr_recv) {
            rdma_error("Failed to create a buffer for receiver\n");
            /* we assume that it is due to out of memory error */
            return -ENOMEM;
        }
        /* This buffer is used to transmit information about the above
         * buffer to the remote. So this contains the metadata about the
         * receiver buffer. Hence this is called metadata buffer. Since this is
         * already on allocated, we just register it. We need to prepare a send
         * I/O operation that will tell the remote the address of the receiver
         * buffer.
         */

        struct rdma_buffer_attr buffer_metadata;
        buffer_metadata.address = (uint64_t)sock->mr_recv->addr;
        buffer_metadata.length = (uint32_t)sock->mr_recv->length;
        buffer_metadata.stag.local_stag = (uint32_t)sock->mr_recv->lkey;

        rdma_buffer_deregister(sock->metadata_mr2);

        sock->metadata_mr2 =
            rdma_buffer_register(sock->pd /* which protection domain*/, &buffer_metadata /* which memory to register */,
                                 sizeof(buffer_metadata) /* what is the size of memory */, IBV_ACCESS_LOCAL_WRITE /* what access permission */);
        if (!sock->metadata_mr2) {
            rdma_error("Receiver failed to create to hold receiver metadata \n");
            /* we assume that this is due to out of memory error */
            return -ENOMEM;
        }

        /* We need to transmit this buffer. So we create a send request.
         * A send request consists of multiple SGE elements. In our case, we
         * only have one
         */
        struct ibv_sge server_send_sge;
        server_send_sge.addr = (uint64_t)&buffer_metadata;
        server_send_sge.length = sizeof(buffer_metadata);
        server_send_sge.lkey = sock->metadata_mr2->lkey;
        /* now we link this sge to the send request */
        struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;

        bzero(&server_send_wr, sizeof(server_send_wr));
        server_send_wr.sg_list = &server_send_sge;
        server_send_wr.num_sge = 1;                     // only 1 SGE element in the array
        server_send_wr.opcode = IBV_WR_SEND;            // This is a send request
        server_send_wr.send_flags = IBV_SEND_SIGNALED;  // We want to get notification
                                                        /* This is a fast data path operation. Posting an I/O request */
        debug("recv: posting our metadata to send\n");

        ret = ibv_post_send(sock->qp /* which QP */, &server_send_wr /* Send request that we prepared before */,
                            &bad_server_send_wr /* In case of error, this will contain failed requests */);
        if (ret) {
            rdma_error("Posting of receiver metdata failed, errno: %d \n", -errno);
            return -errno;
        }

        /* We check for completion notification */
        ret = process_work_completion_events(sock->completion_channel, &wc, 1);
        if (ret != 1) {
            rdma_error("Failed to send receiver metadata, ret = %d \n", ret);
            return ret;
        }
        debug("Local buffer metadata has been sent to the sender \n");

        bzero(&client_recv_wr, sizeof(client_recv_wr));
        debug("recv: posting buffer for send signal to recv\n");
        ret = ibv_post_recv(sock->qp /* which QP */, &client_recv_wr /* receive work request*/, &bad_client_recv_wr /* error WRs */);
        if (ret) {
            rdma_error("Failed to post the receive buffer, errno: %d \n", ret);
            return ret;
        }
        debug("send signal buffer posting is successful \n");

        // Look for a completion of remote side write
        debug("nowait: trying to process WRITE event:\n\n");
        ret = process_work_completion_events(sock->completion_channel, &wc, 1);
        if (ret != 1) {
            rdma_error("We failed to get 1 work completions , ret = %d \n", ret);
            return ret;
        }

        rdma_buffer_deregister(sock->metadata_mr);
        sock->metadata_mr =
            rdma_buffer_register(sock->pd /* which protection domain */, &sock->remote_metadata_attr /* what memory */,
                                 sizeof(sock->remote_metadata_attr) /* what length */, (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
        if (!sock->metadata_mr) {
            rdma_error("Failed to register client attr buffer\n");
            // we assume ENOMEM
            return -ENOMEM;
        }
        /* We post this receive buffer for client metadata on the QP. SGE credentials is where we
         * receive the metadata from the client */
        client_recv_sge.addr = (uint64_t)sock->metadata_mr->addr;  // same as &client_buffer_attr
        client_recv_sge.length = (uint32_t)sock->metadata_mr->length;
        client_recv_sge.lkey = (uint32_t)sock->metadata_mr->lkey;
        /* Now we link this SGE to the work request (WR) */
        bzero(&client_recv_wr, sizeof(client_recv_wr));
        client_recv_wr.sg_list = &client_recv_sge;
        client_recv_wr.num_sge = 1;  // only one SGE
        debug("recv: preposting to recv\n");

        ret = ibv_post_recv(sock->qp /* which QP */, &client_recv_wr /* receive work request*/, &bad_client_recv_wr /* error WRs */);
        if (ret) {
            rdma_error("Failed to post the receive buffer, errno: %d \n", ret);
            return ret;
        }

        if (!(sock->last_bufptr_recv == buf && sock->last_len_recv == len)) {  // we only GC when new buffer is passed
            sock->gc_container[sock->gc_counter] = sock->mr_recv;
            sock->gc_counter++;
            gc_sock(sock);
        }

        sock->last_bufptr_recv = buf;
        sock->last_len_recv = len;

        debug("Receive buffer posting is successful \n");
        return len;
    }

    debug(ANSI_COLOR_RED "Sockfd %d not found, letting go\n" ANSI_COLOR_RESET, fd);

    return _recv(fd, buf, len, flags);
}

int close(int fd) {
    debug(ANSI_COLOR_YELLOW "\tCLOSE CALL\n" ANSI_COLOR_RESET);

    bool is_rdma_sockfd = false;
    if (isEmpty()) {
        return _close(fd);
    }
    sock *sock = find(fd);
    if (sock) {
        printf("calloc: %d, registration %d, post %d\n", num_calloc, registration, post);
        printf("totals:\ncalloc: %f, registration %f, post %f\n", amount_calloc, amount_registration, amount_post);
        // disconnect
        if (sock->qp) {
            rdma_disconnect(sock->cm_id);

            struct rdma_cm_event *cm_event = NULL;
            process_rdma_cm_event(sock->event_channel, RDMA_CM_EVENT_DISCONNECTED, &cm_event);
            rdma_ack_cm_event(cm_event);
        }

        /* Destroy QP */
        if (sock->qp) rdma_destroy_qp(sock->cm_id);
        /* Destroy client cm id */
        /* Destroy CQ */
        if (sock->cq) ibv_destroy_cq(sock->cq);
        /* Destroy completion channel */
        if (sock->completion_channel) ibv_destroy_comp_channel(sock->completion_channel);
        if (sock->event_channel) rdma_destroy_event_channel(sock->event_channel);

        /* Destroy memory buffers */
        for (size_t i = 0; i < GC_NUM; i++) {
            rdma_buffer_deregister(sock->gc_container[i]);
        }

        if (sock->metadata_mr) rdma_buffer_deregister(sock->metadata_mr);
        if (sock->metadata_mr2) rdma_buffer_deregister(sock->metadata_mr2);
        if (sock->local_buf) rdma_buffer_free(sock->local_buf);

        /* Destroy protection domain */
        if (sock->pd) ibv_dealloc_pd(sock->pd);

        delete (fd);  // remove socket from linked list

        debug("Resource clean up is complete \n");
        return 0;
    }
    return _close(fd);
}

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    debug(ANSI_COLOR_YELLOW "\tCONNECT CALL\n" ANSI_COLOR_RESET);

    bool is_rdma_sockfd = false;  // omfg
    if (isEmpty()) {
        return _close(fd);
    }

    sock *sock = find(fd);

    char *ip = inet_ntoa(((struct sockaddr_in *)addr)->sin_addr);
    if (sock) {
        debug("cm id: %i, remote ip: %s\n", sock->cm_id, ip);

        int ret = rdma_resolve_addr(sock->cm_id, NULL, (struct sockaddr *)addr, 2000);
        struct rdma_cm_event *cm_event = NULL;
        if (ret) {
            rdma_error("Failed to resolve address, errno: %d \n", -errno);
            return -errno;
        }
        debug("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n");
        ret = process_rdma_cm_event(sock->event_channel, RDMA_CM_EVENT_ADDR_RESOLVED, &cm_event);
        if (ret) {
            rdma_error("Failed to receive a valid event, ret = %d \n", ret);
            return ret;
        }
        /* we ack the event */
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
            rdma_error("Failed to acknowledge the CM event, errno: %d\n", -errno);
            return -errno;
        }
        debug("RDMA address is resolved \n");

        /* Resolves an RDMA route to the destination address in order to
         * establish a connection */
        ret = rdma_resolve_route(sock->cm_id, 2000);
        if (ret) {
            rdma_error("Failed to resolve route, erno: %d \n", -errno);
            return -errno;
        }
        debug("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
        ret = process_rdma_cm_event(sock->event_channel, RDMA_CM_EVENT_ROUTE_RESOLVED, &cm_event);
        if (ret) {
            rdma_error("Failed to receive a valid event, ret = %d \n", ret);
            return ret;
        }
        /* we ack the event */
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
            rdma_error("Failed to acknowledge the CM event, errno: %d \n", -errno);
            return -errno;
        }

        /* Protection Domain (PD) is similar to a "process abstraction"
         * in the operating system. All resources are tied to a particular PD.
         * And accessing recourses across PD will result in a protection fault.
         */
        sock->pd = ibv_alloc_pd(sock->cm_id->verbs);
        if (!sock->pd) {
            rdma_error("Failed to alloc pd, errno: %d \n", -errno);
            return -errno;
        }
        debug("pd allocated at %p \n", sock->pd);
        /* Now we need a completion channel, were the I/O completion
         * notifications are sent. Remember, this is different from connection
         * management (CM) event notifications.
         * A completion channel is also tied to an RDMA device, hence we will
         * use cm_host_id->verbs.
         */
        sock->completion_channel = ibv_create_comp_channel(sock->cm_id->verbs);
        if (!sock->completion_channel) {
            rdma_error("Failed to create IO completion event channel, errno: %d\n", -errno);
            return -errno;
        }
        debug("completion event channel created at : %p \n", sock->completion_channel);
        /* Now we create a completion queue (CQ) where actual I/O
         * completion metadata is placed. The metadata is packed into a
         * structure called struct ibv_wc (wc = work completion). ibv_wc has
         * detailed information about the work completion. An I/O request in
         * RDMA world is called "work" ;)
         */
        sock->cq = ibv_create_cq(sock->cm_id->verbs /* which device*/, CQ_CAPACITY /* maximum capacity*/, NULL /* user context, not used here */,
                                 sock->completion_channel /* which IO completion channel */, 0 /* signaling vector, not used here*/);
        if (!sock->cq) {
            rdma_error("Failed to create CQ, errno: %d \n", -errno);
            return -errno;
        }
        debug("CQ created at %p with %d elements \n", sock->cq, sock->cq->cqe);
        ret = ibv_req_notify_cq(sock->cq, 0);
        if (ret) {
            rdma_error("Failed to request notifications, errno: %d\n", -errno);
            return -errno;
        }
        /* Now the last step, set up the queue pair (send, recv) queues and
         * their capacity. The capacity here is define statically but this can
         * be probed from the device. We just use a small number as defined in
         * rdma_common.h */
        struct ibv_qp_init_attr qp_init_attr;
        bzero(&qp_init_attr, sizeof qp_init_attr);
        qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
        qp_init_attr.cap.max_recv_wr = MAX_WR;   /* Maximum receive posting capacity */
        qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
        qp_init_attr.cap.max_send_wr = MAX_WR;   /* Maximum send posting capacity */
        qp_init_attr.qp_type = IBV_QPT_RC;       /* QP type, RC = Reliable connection */
        /* We use same completion queue, but one can use different queues */
        qp_init_attr.recv_cq = sock->cq; /* Where should I notify for receive
                                             completion operations */
        qp_init_attr.send_cq = sock->cq; /* Where should I notify for send
                                             completion operations */
        /*Lets create a QP */
        ret = rdma_create_qp(sock->cm_id /* which connection id */, sock->pd /* which protection domain*/, &qp_init_attr /* Initial attributes */);
        if (ret) {
            rdma_error("Failed to create QP, errno: %d \n", -errno);
            return -errno;
        }
        sock->qp = sock->cm_id->qp;
        debug("QP created at %p \n", sock->qp);

        struct rdma_conn_param conn_param;
        ret = -1;
        bzero(&conn_param, sizeof(conn_param));
        conn_param.initiator_depth = 7;
        conn_param.responder_resources = 7;
        conn_param.retry_count = 7;  // if fail, then how many times to retry
        conn_param.rnr_retry_count = 7;
        ret = rdma_connect(sock->cm_id, &conn_param);
        if (ret) {
            rdma_error("Failed to connect to remote host , errno: %d\n", -errno);
            return -errno;
        }
        debug("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED\n");
        ret = process_rdma_cm_event(sock->event_channel, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
        if (ret) {
            rdma_error("Failed to get cm event, ret = %d \n", ret);
            return ret;
        }
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
            rdma_error("Failed to acknowledge cm event, errno: %d\n", -errno);
            return -errno;
        }
        debug("The client is connected successfully \n");

        // this will hopefully exchange the receive buffer metadata
        exchange_metadata(sock);

        // // pre-post receive buffer
        // ret = -1;
        // sock->metadata_mr =
        //     rdma_buffer_register(sock->pd, &sock->remote_metadata_attr, sizeof(sock->remote_metadata_attr), (IBV_ACCESS_LOCAL_WRITE));
        // if (!sock->metadata_mr) {
        //     rdma_error("Failed to setup the server metadata mr , -ENOMEM\n");
        //     return -ENOMEM;
        // }
        // struct ibv_sge server_recv_sge;
        // struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr;
        // server_recv_sge.addr = (uint64_t)sock->metadata_mr->addr;
        // server_recv_sge.length = (uint32_t)sock->metadata_mr->length;
        // server_recv_sge.lkey = (uint32_t)sock->metadata_mr->lkey;
        // /* now we link it to the request */
        // bzero(&server_recv_wr, sizeof(server_recv_wr));
        // server_recv_wr.sg_list = &server_recv_sge;
        // server_recv_wr.num_sge = 1;
        // debug("connect: preposting to recv\n");

        // ret = ibv_post_recv(sock->qp /* which QP */, &server_recv_wr /* receive work request*/, &bad_server_recv_wr /* error WRs */);
        // if (ret) {
        //     rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
        //     return ret;
        // }
        // debug("Receive buffer pre-posting is successful \n");
        return ret;

        return -ENOSYS;
    }
    return _connect(fd, addr, addrlen);
}

void _function_override_init() {
    __start_main = dlsym(RTLD_NEXT, "__libc_start_main");
    _socket = dlsym(RTLD_NEXT, "socket");
    _connect = dlsym(RTLD_NEXT, "connect");
    _accept = dlsym(RTLD_NEXT, "accept");
    _bind = dlsym(RTLD_NEXT, "bind");
    _listen = dlsym(RTLD_NEXT, "listen");
    _send = dlsym(RTLD_NEXT, "send");
    _recv = dlsym(RTLD_NEXT, "recv");
    _close = dlsym(RTLD_NEXT, "close");
    _setsockopt = dlsym(RTLD_NEXT, "setsockopt");
    _getsockopt = dlsym(RTLD_NEXT, "getsockopt");
    _select = dlsym(RTLD_NEXT, "select");
    _read = dlsym(RTLD_NEXT, "read");
}
