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

// XXX: _GNU_SOURCE must be defined before including dlfcn to get RTLD_NEXT
// symbols
#define _GNU_SOURCE

#include "anpwrapper.h"

#include <dlfcn.h>

#include "all_common.h"
#include "init.h"
#include "linklist.h"
#include "rdma_common.h"
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

static int (*_listen)(int __fd, int __n) = NULL;
static int (*_setsockopt)(int __fd, int __level, int __optname, const void *__optval, socklen_t __optlen) = NULL;
static int (*_getsockopt)(int __fd, int __level, int __optname, void *__restrict__ __optval, socklen_t *__restrict__ __optlen) = NULL;
static int (*_select)(int __nfds, fd_set *__restrict__ __readfds, fd_set *__restrict__ __writefds, fd_set *__restrict__ __exceptfds,
                      struct timeval *__restrict__ __timeout) = NULL;

static ssize_t (*_read)(int __fd, void *__buf, size_t __nbytes) = NULL;

typedef struct fd_resources {
    /// server stuff
    /* These are the RDMA resources needed to setup an RDMA connection */
    /* Event channel, where connection management (cm) related events are relayed */
    int fd;
    struct rdma_cm_id *cm_id;
    struct rdma_event_channel *event_channel;
    struct ibv_pd *pd;
    struct ibv_comp_channel *completion_channel;
    struct ibv_cq *cq;
    struct ibv_qp *qp;

    /* RDMA memory resources */
    struct rdma_buffer_attr remote_metadata_attr;

    /* These are memory buffers */
    struct ibv_mr *mr;

    /// hack stuff...
    char *destination_noalloc;

} fd_resources;

fd_resources fds[256];
int fd_count = -1;

ssize_t read(int __fd, void *__buf, size_t __nbytes) {
    // check against our stored fds, if we don't have it let it go, otherwise use our recv
    if (fd_count == -1) {
        return _read(__fd, __buf, __nbytes);
    }
    if (__fd == 0) return recv(__fd, __buf, __nbytes, 0);
    for (size_t i = 0; i <= fd_count; i++) {
        if (fds[i].fd == __fd) {  // TODO check this against our stored FDs
            debug("fd %d matches fd %d\n", fds[i].fd, __fd);
            return recv(__fd, __buf, __nbytes, 0);
        }
    }
    return _read(__fd, __buf, __nbytes);
}

const char *mapStatusToType(enum ibv_wc_status status) {
    switch (status) {
        case IBV_WC_SUCCESS:
            return "IBV_WC_SUCCESS";
        case IBV_WC_LOC_LEN_ERR:
            return "IBV_WC_LOC_LEN_ERR";
        case IBV_WC_LOC_QP_OP_ERR:
            return "IBV_WC_LOC_QP_OP_ERR";
        case IBV_WC_LOC_EEC_OP_ERR:
            return "IBV_WC_LOC_EEC_OP_ERR";
        case IBV_WC_LOC_PROT_ERR:
            return "IBV_WC_LOC_PROT_ERR";
        case IBV_WC_WR_FLUSH_ERR:
            return "IBV_WC_WR_FLUSH_ERR";
        case IBV_WC_MW_BIND_ERR:
            return "IBV_WC_MW_BIND_ERR";
        case IBV_WC_BAD_RESP_ERR:
            return "IBV_WC_BAD_RESP_ERR";
        case IBV_WC_LOC_ACCESS_ERR:
            return "IBV_WC_LOC_ACCESS_ERR";
        case IBV_WC_REM_INV_REQ_ERR:
            return "IBV_WC_REM_INV_REQ_ERR";
        case IBV_WC_REM_ACCESS_ERR:
            return "IBV_WC_REM_ACCESS_ERR";
        case IBV_WC_REM_OP_ERR:
            return "IBV_WC_REM_OP_ERR";
        case IBV_WC_RETRY_EXC_ERR:
            return "IBV_WC_RETRY_EXC_ERR";
        case IBV_WC_RNR_RETRY_EXC_ERR:
            return "IBV_WC_RNR_RETRY_EXC_ERR";
        case IBV_WC_LOC_RDD_VIOL_ERR:
            return "IBV_WC_LOC_RDD_VIOL_ERR";
        case IBV_WC_REM_INV_RD_REQ_ERR:
            return "IBV_WC_REM_INV_RD_REQ_ERR";
        case IBV_WC_REM_ABORT_ERR:
            return "IBV_WC_REM_ABORT_ERR";
        case IBV_WC_INV_EECN_ERR:
            return "IBV_WC_INV_EECN_ERR";
        case IBV_WC_INV_EEC_STATE_ERR:
            return "IBV_WC_INV_EEC_STATE_ERR";
        case IBV_WC_FATAL_ERR:
            return "IBV_WC_FATAL_ERR";
        case IBV_WC_RESP_TIMEOUT_ERR:
            return "IBV_WC_RESP_TIMEOUT_ERR";
        case IBV_WC_GENERAL_ERR:
            return "IBV_WC_GENERAL_ERR";
    }
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

/* This is our testing function */
static int check_src_dst(char *source, char *destination, ssize_t len) { return memcmp((void *)source, (void *)destination, len); }

int setsockopt(int __fd, int __level, int __optname, const void *__optval, socklen_t __optlen) {  // hack to get it to succeed
    debug(ANSI_COLOR_YELLOW "\tSETSOCKOPT CALL\n" ANSI_COLOR_RESET);
    return 0;
}

int getsockopt(int __fd, int __level, int __optname, void *__restrict__ __optval, socklen_t *__restrict__ __optlen) { return 0; }

int select(int __nfds, fd_set *__restrict__ __readfds, fd_set *__restrict__ __writefds, fd_set *__restrict__ __exceptfds,
           struct timeval *__restrict__ __timeout) {
    debug(ANSI_COLOR_YELLOW "\tSELECT CALL: %d\n" ANSI_COLOR_RESET, fds[fd_count].fd);
    if (fds[fd_count].fd != NULL) return 1;
    return -1;
}
#define this fds[fd_count]

int socket(int domain, int type, int protocol) {
    debug(ANSI_COLOR_YELLOW "\tSOCKET CALL\n" ANSI_COLOR_RESET);

    if (is_socket_supported(domain, type, protocol)) {
        int ret = -1;
        fd_count++;
        /*  Open a channel used to report asynchronous communication event */
        this.event_channel = rdma_create_event_channel();

        if (!this.event_channel) {
            rdma_error("Creating cm event channel failed with errno : (%d)", -errno);
            return -errno;
        }
        debug("RDMA CM event channel is created successfully at %p \n", this.event_channel);
        /* rdma_cm_id is the connection identifier (like socket) which is used
         * to define an RDMA connection.
         */
        ret = rdma_create_id(this.event_channel, &this.cm_id, NULL, RDMA_PS_TCP);
        debug(ANSI_COLOR_YELLOW "\tRDMA ID created\n" ANSI_COLOR_RESET);
        if (ret) {
            rdma_error("Creating cm id failed with errno: %d ", -errno);
            return -errno;
        }

        debug("socket made with id %d\n", this.cm_id);
        this.fd = this.cm_id;
        return this.cm_id;
    }
    return _socket(domain, type, protocol);
}

int listen(int __fd, int __n) {
    debug(ANSI_COLOR_YELLOW "\tLISTEN CALL\n" ANSI_COLOR_RESET);

    bool is_anp_sockfd = false;
    fd_resources *sock;

    for (int i = 0; i <= fd_count; i++) {
        debug(ANSI_COLOR_GREEN "%d == %d\n" ANSI_COLOR_RESET, fds[i].fd, __fd);
        if (fds[i].fd == __fd) {
            is_anp_sockfd = true;
            sock = &fds[i];
        }
    }
    if (is_anp_sockfd) {
        debug("listen with fd %d\n", __fd);
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
        // debug("Server is listening successfully at: %s , port: %d \n",
        // 		inet_ntoa(sockaddr->sin_addr), // we don't pass this, todo as fd
        // 		ntohs(sockaddr->sin_port));

        return ret;
    }
    return _listen(__fd, __n);
}

int accept(int socket, struct sockaddr *restrict address,
           socklen_t *restrict address_len) {  // TODO: use args instead of
                                               // ignoring them

    // setup client resources
    debug(ANSI_COLOR_YELLOW "\tACCEPT CALL\n" ANSI_COLOR_RESET);

    bool is_anp_sockfd = false;
    fd_resources *sock;

    for (int i = 0; i <= fd_count; i++) {
        debug(ANSI_COLOR_GREEN "%d == %d\n" ANSI_COLOR_RESET, fds[i].fd, socket);
        if (fds[i].fd == socket) {
            is_anp_sockfd = true;
            sock = &fds[i];
        }
    }
    if (is_anp_sockfd) {
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
        fd_count++;
        fds[fd_count] = fds[fd_count - 1];  // inherit stuff from the other socket
        sock = &fds[fd_count];
        sock->cm_id = cm_event->id;
        sock->fd = cm_event->id;
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
        debug("A new RDMA client connection id is stored at %p\n", sock->cm_id);

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
        sock->pd = ibv_alloc_pd(sock->cm_id->verbs
	/* verbs defines a verb's provider,
		* i.e an RDMA device where the incoming
		* client connection came */);
        if (!sock->pd) {
            rdma_error("Failed to allocate a protection domain errno: %d\n", -errno);
            return -errno;
        }
        debug("A new protection domain is allocated at %p \n", sock->pd);
        /* Now we need a completion channel, were the I/O completion
         * notifications are sent. Remember, this is different from connection
         * management (CM) event notifications.
         * A completion channel is also tied to an RDMA device, hence we will
         * use cm_host_id->verbs.
         */
        sock->completion_channel = ibv_create_comp_channel(sock->cm_id->verbs);
        if (!sock->completion_channel) {
            rdma_error("Failed to create an I/O completion event channel, %d\n", -errno);
            return -errno;
        }
        debug("An I/O completion event channel is created at %p \n", sock->completion_channel);
        /* Now we create a completion queue (CQ) where actual I/O
         * completion metadata is placed. The metadata is packed into a structure
         * called struct ibv_wc (wc = work completion). ibv_wc has detailed
         * information about the work completion. An I/O request in RDMA world
         * is called "work" ;)
         */
        sock->cq = ibv_create_cq(sock->cm_id->verbs /* which device*/, CQ_CAPACITY /* maximum capacity*/, NULL /* user context, not used here */,
                                 sock->completion_channel /* which IO completion channel */, 0 /* signaling vector, not used here*/);
        if (!sock->cq) {
            rdma_error("Failed to create a completion queue (cq), errno: %d\n", -errno);
            return -errno;
        }
        debug("Completion queue (CQ) is created at %p with %d elements \n", sock->cq, sock->cq->cqe);
        /* Ask for the event for all activities in the completion queue*/
        ret = ibv_req_notify_cq(sock->cq /* on which CQ */, 0 /* 0 = all event type, no filter*/);
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
        qp_init_attr.recv_cq = sock->cq; /* Where should I notify for receive completion operations */
        qp_init_attr.send_cq = sock->cq; /* Where should I notify for send completion operations */

        /*Lets create a QP */
        ret = rdma_create_qp(sock->cm_id /* which connection id */, sock->pd /* which protection domain*/, &qp_init_attr /* Initial attributes */);
        if (ret) {
            rdma_error("Failed to create QP due to errno: %d\n", -errno);
            return -errno;
        }
        /* Save the reference for handy typing but is not required */
        sock->qp = sock->cm_id->qp;
        debug("Client QP created at %p\n", sock->qp);

#pragma endregion

        // accept
        struct rdma_conn_param conn_param;
        struct sockaddr_in remote_sockaddr;

        memset(&conn_param, 0, sizeof(conn_param));
        /* this tell how many outstanding requests can we handle */
        conn_param.initiator_depth = 3; /* For this exercise, we put a small number here */
        /* This tell how many outstanding requests we expect other side to handle */
        conn_param.responder_resources = 3; /* For this exercise, we put a small number */
        conn_param.rnr_retry_count = 7;
        ret = rdma_accept(sock->cm_id, &conn_param);
        if (ret) {
            rdma_error("Failed to accept the connection, errno: %d \n", -errno);
            return -errno;
        }
        /* We expect an RDMA_CM_EVNET_ESTABLISHED to indicate that the RDMA
         * connection has been established and everything is fine on both, server
         * as well as the client sides.
         */
        debug("Going to wait for RDMA_CM_EVENT_ESTABLISHED event \n");

        ret = process_rdma_cm_event(sock->event_channel, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
        if (ret) {
            rdma_error("Failed to get the cm event, errnp: %d \n", -errno);
            return -errno;
        }
        debug(ANSI_COLOR_CYAN "accept new id: %d\n" ANSI_COLOR_RESET, sock->cm_id);
        // sock->fd = sock->c/m_id; // TODO make this a pointer so C can actually write to it xD
        /* We acknowledge the event */
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
            rdma_error("Failed to acknowledge the cm event %d\n", -errno);
            return -errno;
        }

        static struct ibv_sge remote_recv_sge;
        static struct ibv_recv_wr remote_recv_wr, *bad_remote_recv_wr = NULL;
        // static struct ibv_mr *client_metadata_mr = NULL;
        sock->mr = rdma_buffer_register(sock->pd /* which protection domain */, &sock->remote_metadata_attr /* what memory */,
                                        sizeof(sock->remote_metadata_attr) /* what length */, (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
        if (!&sock->remote_metadata_attr) {
            rdma_error("Failed to register client attr buffer\n");
            // we assume ENOMEM
            return -ENOMEM;
        }

        /* We pre-post this receive buffer for client metadata on the QP. SGE credentials is where we
         * receive the metadata from the client */
        remote_recv_sge.addr = (uint64_t)sock->mr->addr;
        remote_recv_sge.length = sock->mr->length;
        remote_recv_sge.lkey = sock->mr->lkey;
        /* Now we link this SGE to the work request (WR) */
        bzero(&remote_recv_wr, sizeof(remote_recv_wr));
        remote_recv_wr.sg_list = &remote_recv_sge;
        remote_recv_wr.num_sge = 1;  // only one SGE
        debug("accept: preposting buffer for client metadata to recv\n");
        ret = ibv_post_recv(sock->qp /* which QP */, &remote_recv_wr /* receive work request*/, &bad_remote_recv_wr /* error WRs */);
        if (ret) {
            rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
            return ret;
        }
        debug("Receive buffer pre-posting is successful \n");

        return sock->fd;

        // TODO return cm_host_id here (as new socket)
    }
    return _accept(socket, address, address_len);
}

int bind(int socket, const struct sockaddr *server_sockaddr,
         socklen_t address_len) {  // address_len is useless here because of RDMA

    debug(ANSI_COLOR_YELLOW "\tBIND CALL\n" ANSI_COLOR_RESET);

    bool is_anp_sockfd = false;
    fd_resources *sock;

    for (int i = 0; i <= fd_count; i++) {
        debug(ANSI_COLOR_GREEN "%d == %d\n" ANSI_COLOR_RESET, fds[i].fd, socket);
        if (fds[i].fd == socket) {
            is_anp_sockfd = true;
            sock = &fds[i];
        }
    }
    if (is_anp_sockfd) {
        char *ip = inet_ntoa(((struct sockaddr_in *)server_sockaddr)->sin_addr);
        debug("Trying to bind %s with socket %d\n", ip, sock->cm_id);
        int ret = rdma_bind_addr(sock->cm_id, (struct sockaddr *)server_sockaddr);
        if (ret) {
            rdma_error("Failed to bind server address, errno: %d \n", -errno);
            return -errno;
        }
        return ret;
    }
    return _bind(socket, server_sockaddr, address_len);
}

int not_first = 0;
ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    // FIXME -- you can remember the file descriptors that you have generated in
    // the socket call and match them here
    debug(ANSI_COLOR_YELLOW "\tSEND CALL\n" ANSI_COLOR_RESET);

    bool is_anp_sockfd = false;
    fd_resources *sock;

    if (fd_count == -1) return _send(sockfd, buf, len, flags);
    for (int i = 0; i <= fd_count; i++) {
        debug(ANSI_COLOR_GREEN "%d == %d\n" ANSI_COLOR_RESET, fds[i].fd, sockfd);
        if (fds[i].fd == sockfd) {
            is_anp_sockfd = true;
            sock = &fds[i];
        }
    }

    if (is_anp_sockfd) {  // todo add proper socket check
        // rdma_send:

        // post receive buffer
        int ret = -1;

        // allocate destination buffer -- ?
        // if (!sock->destination_noalloc) {
        //     printf(ANSI_COLOR_RED "expensive\n" ANSI_COLOR_RESET);
        //     sock->destination_noalloc = calloc(len, 1);
        // }
        // // TODO add back in:
        // if (sizeof(destination_noalloc)/sizeof(destination_noalloc[0]) < len) {
        // 	printf(ANSI_COLOR_RED"expensive2 %d\n"ANSI_COLOR_RESET, sizeof(destination_noalloc)/sizeof(destination_noalloc[0]));
        // 	destination_noalloc = realloc(destination_noalloc, len);
        // // }
        // if (!sock->destination_noalloc) {
        //     rdma_error("Failed to allocate destination memory, -ENOMEM\n");
        //     // free(src);
        //     return -ENOMEM;
        // }

        // send(): exchange metadata with remote side
        struct ibv_wc wc[2];

        // printf(ANSI_COLOR_RED "expensive3\n" ANSI_COLOR_RESET);


		/// register RDMA buffer for the stuff we're sending
		if (sock->mr->addr != buf || sock->mr->length != len) {
			printf("ummmm %d == %d, %d == %d\n", sock->mr->addr, buf, sock->mr->length, len);
        	sock->mr = rdma_buffer_register(sock->pd, buf, len, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));
		}
        if (!sock->mr) {
            rdma_error("Failed to register the data buffer, ret = %d \n", ret);
            return ret;
        }

        // TODO refactor out because expensive (29us!)
        struct rdma_buffer_attr local_metadata;
        /* we prepare metadata for the data buffer */
        local_metadata.address = (uint64_t)sock->mr->addr;
        local_metadata.length = sock->mr->length;
        local_metadata.stag.local_stag = sock->mr->lkey;

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
        /* at this point we are expecting 2 work completion. One for our
         * send and one for recv that we will get from the server for
         * its buffer information */
        debug("process work\n");
        ret = process_work_completion_events(sock->completion_channel, wc, 2);
        if (ret != 2) {
            rdma_error("We failed to get 2 work completions , ret = %d \n", ret);
            return ret;
        }
        debug("Remote sent us its buffer location and credentials, showing \n");
        // TODO remove for latency
        show_rdma_buffer_attr(&sock->remote_metadata_attr);

        // remote mem ops
        ret = -1;
        // if (!sock->mr) {
        // 	printf("this never runs\n");
        //     sock->mr = rdma_buffer_register(sock->pd, sock->destination_noalloc, len,
        //                                    (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ));
        //     if (!sock->mr) {
        //         rdma_error("We failed to create the destination buffer, -ENOMEM\n");
        //         return -ENOMEM;
        //     }
        // }
        /* Step 1: is to copy the local buffer into the remote buffer. We will
         * reuse the previous variables. */
        /* now we fill up SGE */
        send_sge.addr = (uint64_t)sock->mr->addr;
        send_sge.length = (uint32_t)sock->mr->length;
        send_sge.lkey = sock->mr->lkey;
        /* now we link to the send work request */
        // bzero(&send_wr, sizeof(send_wr));
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;
        send_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
        send_wr.send_flags = IBV_SEND_SIGNALED;
        /* we have to give info for RDMA */
        send_wr.wr.rdma.rkey = sock->remote_metadata_attr.stag.remote_stag;  // TODO this is a client side struct, should be
                                                                             // made generic
        send_wr.wr.rdma.remote_addr = sock->remote_metadata_attr.address;    // TODO this is a client side struct,
                                                                             // should be made generic
                                                                             /* Now we post it */
        debug("posting our buffer to send\n");

        ret = ibv_post_send(sock->qp, &send_wr, &bad_send_wr);
        if (ret) {
            rdma_error("Failed to write client source buffer, errno: %d \n", -errno);
            return -errno;
        }
        /* at this point we are expecting 1 work completion for the write */
        ret = process_work_completion_events(sock->completion_channel, &wc, 1);
        if (ret != 1) {
            rdma_error("We failed to get 1 work completions , ret = %d \n", ret);
            return ret;
        }

        debug("Client side WRITE is complete \n");

        // struct ibv_mr *remote_metadata_mr;
        // struct ibv_sge server_recv_sge;
        static struct ibv_recv_wr recv_wr, *bad_recv_wr = NULL;

        metadata_mr = rdma_buffer_register(sock->pd, &sock->remote_metadata_attr, sizeof(sock->remote_metadata_attr), (IBV_ACCESS_LOCAL_WRITE));
        if (!metadata_mr) {
            rdma_error("Failed to setup the server metadata mr , -ENOMEM\n");
            return -ENOMEM;
        }
        send_sge.addr = (uint64_t)metadata_mr->addr;
        send_sge.length = (uint32_t)metadata_mr->length;
        send_sge.lkey = (uint32_t)metadata_mr->lkey;
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
		// if (sock->mr) {
		// 	// printf("deregistering\n");
			rdma_buffer_deregister(sock->mr);
		// }

        return len;
    }
    // the default path
    return _send(sockfd, buf, len, flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    debug(ANSI_COLOR_YELLOW "\tRECV CALL\n" ANSI_COLOR_RESET);

    bool is_anp_sockfd = false;

    fd_resources *sock;

    if (fd_count == -1) return _recv(sockfd, buf, len, flags);
    for (int i = 0; i <= fd_count; i++) {
        debug(ANSI_COLOR_GREEN "%d == %d\n" ANSI_COLOR_RESET, fds[i].fd, sockfd);
        if (fds[i].fd == sockfd) {
            is_anp_sockfd = true;
            sock = &fds[i];
        }
    }
    if (is_anp_sockfd) {
        int ret = -1;

        static struct ibv_sge client_recv_sge;
        static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
        struct rdma_cm_event *cm_event = NULL;

        // if (!cm_host_id || !qp) {
        //     rdma_error("Client resources are not properly setup\n");
        //     return -EINVAL;
        // }
        /* we prepare the receive buffer in which we will receive the client
         * metadata*/
        // }
        // send host metadata to remote
        struct ibv_wc wc;
        ret = -1;
        /* Now, we first wait for the remote to start the communication by
         * sending the host its metadata info. The host does not use it
         * in our example. We will receive a work completion notification for
         * our pre-posted receive request.
         */
        debug("process work\n");
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
        // if (!host_mr) {
        sock->mr = rdma_buffer_alloc(sock->pd /* which protection domain */, sock->remote_metadata_attr.length /* what size to allocate */,
                                     (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE), /* access permissions */
                                     buf);
        // }
        // if (host_mr->length < remote_metadata_attr.length) {
        //     rdma_buffer_deregister(host_mr);
        //     host_mr =
        //         rdma_buffer_alloc(pd /* which protection domain */, remote_metadata_attr.length /* what size to allocate */,
        //                           (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE), /* access permissions */
        //                           buf);
        // }
        if (!sock->mr) {
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
        struct rdma_buffer_attr receiver_metadata_attr;
        receiver_metadata_attr.address = (uint64_t)sock->mr->addr;
        receiver_metadata_attr.length = (uint32_t)sock->mr->length;
        receiver_metadata_attr.stag.local_stag = (uint32_t)sock->mr->lkey;
        sock->mr = rdma_buffer_register(sock->pd /* which protection domain*/, &receiver_metadata_attr /* which memory to register */,
                                        sizeof(receiver_metadata_attr) /* what is the size of memory */,
                                        IBV_ACCESS_LOCAL_WRITE /* what access permission */);
        if (!sock->mr) {
            rdma_error("Receiver failed to create to hold receiver metadata \n");
            /* we assume that this is due to out of memory error */
            return -ENOMEM;
        }

        /* We need to transmit this buffer. So we create a send request.
         * A send request consists of multiple SGE elements. In our case, we
         * only have one
         */
        struct ibv_sge server_send_sge;
        server_send_sge.addr = (uint64_t)&receiver_metadata_attr;
        server_send_sge.length = sizeof(receiver_metadata_attr);
        server_send_sge.lkey = sock->mr->lkey;
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

        // nowait
        // post receive request for send completion signal

        static struct ibv_mr *send_signal_mr = NULL;
        // client_recv_sge.addr = (uint64_t)send_signal_mr->addr;  // same as &client_buffer_attr
        // client_recv_sge.length = send_signal_mr->length;
        // client_recv_sge.lkey = send_signal_mr->lkey;
        /* Now we link this SGE to the work request (WR) */
        bzero(&client_recv_wr, sizeof(client_recv_wr));
        // client_recv_wr.sg_list = &client_recv_sge;
        // client_recv_wr.num_sge = 1;  // only one SGE
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

        // prepost for next recv
        // TODONOW i'm pretty sure it's here
        static struct ibv_mr *client_metadata_mr = NULL;  // TODO: rename as remote_metadata_mr

        client_metadata_mr =
            rdma_buffer_register(sock->pd /* which protection domain */, &sock->remote_metadata_attr /* what memory */,
                                 sizeof(sock->remote_metadata_attr) /* what length */, (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
        if (!client_metadata_mr) {
            rdma_error("Failed to register client attr buffer\n");
            // we assume ENOMEM
            return -ENOMEM;
        }
        /* We post this receive buffer for client metadata on the QP. SGE credentials is where we
         * receive the metadata from the client */
        client_recv_sge.addr = (uint64_t)client_metadata_mr->addr;  // same as &client_buffer_attr
        client_recv_sge.length = (uint32_t)client_metadata_mr->length;
        client_recv_sge.lkey = (uint32_t)client_metadata_mr->lkey;
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

        debug("Receive buffer posting is successful \n");
        return len;
    }

    // the default path
    printf(ANSI_COLOR_RED "Sockfd %d not found, letting go\n" ANSI_COLOR_RESET, sockfd);

    return _recv(sockfd, buf, len, flags);
}

int close(int sockfd) {
    debug(ANSI_COLOR_YELLOW "\tCLOSE CALL\n" ANSI_COLOR_RESET);

    // todoremove: structs check
    // if (pd != NULL && cm_host_id != NULL && cm_host_id != NULL)
    //     debug("host match: %d, remote match: %d\n", pd == cm_host_id->pd, pd == cm_host_id->pd);
    // if (cm_event_channel != NULL && cm_host_id != NULL) debug("%d\n", cm_event_channel == cm_host_id->channel);
    // if (qp != NULL && cm_host_id != NULL) debug("%d\n", qp == cm_host_id->qp);
    // if (cq != NULL && cm_host_id != NULL) debug("%d\n", cq == cm_host_id->send_cq || cq == cm_host_id->recv_cq);

    // FIXME -- you can remember the file descriptors that you have generated in
    // the socket call and match them here
    bool is_anp_sockfd = false;
    if (fd_count == -1) {
        return _close(sockfd);
    }
    for (int i = 0; i <= fd_count; i++) {
        debug(ANSI_COLOR_GREEN "%d == %d\n" ANSI_COLOR_RESET, fds[i].fd, sockfd);
        if (fds[i].fd == sockfd) {
            is_anp_sockfd = true;
        }
    }
    if (is_anp_sockfd) {
		fd_resources sock;
        // struct ibv_wc wc;
        // int ret = process_work_completion_events(io_completion_channel, &wc, 1);
        // if (ret) {
        //     debug("not sure what to do here %d", -errno);
        // }
        // close code
        // if (cm_host_id == NULL && cm_host_id != NULL) rdma_disconnect(cm_host_id);  // hacky way to check if we are server or client (remove)
        // struct rdma_cm_event *cm_event = NULL;
        // if (cm_host_id != NULL) process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_DISCONNECTED, &cm_event);
        // rdma_ack_cm_event(cm_event);
        // /* Destroy QP */
        // rdma_destroy_qp(cm_host_id);
        // /* Destroy client cm id */
        // rdma_destroy_id(cm_host_id);
        // if (cm_host_id != NULL) rdma_destroy_id(cm_host_id);
        // /* Destroy CQ */
        // ibv_destroy_cq(client_cq);
        // /* Destroy completion channel */
        // ibv_destroy_comp_channel(io_completion_channel);
        // /* Destroy memory buffers */
        // if (server_metadata_mr != NULL) rdma_buffer_deregister(server_metadata_mr);
        // if (client_src_mr != NULL) rdma_buffer_deregister(client_src_mr);
        // if (client_dst_mr != NULL) rdma_buffer_deregister(client_dst_mr);

        // if (server_buffer_mr != NULL) rdma_buffer_free(server_buffer_mr);

        /* Destroy protection domain */
        // ibv_dealloc_pd(pd);
        // rdma_destroy_event_channel(cm_event_channel);
        // debug("Resource clean up is complete \n");
        return 0;
    }
    // the default path
    return _close(sockfd);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    debug(ANSI_COLOR_YELLOW "\tCONNECT CALL\n" ANSI_COLOR_RESET);

    // FIXME -- you can remember the file descriptors that you have generated in
    // the socket call and match them here
    bool is_anp_sockfd = false;  // omfg
    if (fd_count == -1) {
        return _close(sockfd);
    }

    fd_resources *sock;
    for (int i = 0; i <= fd_count; i++) {
        debug(ANSI_COLOR_GREEN "%d == %d\n" ANSI_COLOR_RESET, fds[i].fd, sockfd);

        if (fds[i].fd == sockfd) {
            sock = &fds[i];
            is_anp_sockfd = true;
        }
    }

    char *ip = inet_ntoa(((struct sockaddr_in *)addr)->sin_addr);
    debug("cm id: %i, remote ip: %s\n", sock->cm_id, ip);
    if (is_anp_sockfd) {
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
        // debug("Trying to connect to server at : %s port: %d \n",
        // 		inet_ntoa(sockaddr->sin_addr),
        // 		ntohs(sockaddr->sin_port));
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
        conn_param.initiator_depth = 3;
        conn_param.responder_resources = 3;
        conn_param.retry_count = 3;  // if fail, then how many times to retry
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

        // pre-post receive buffer
        ret = -1;
        sock->mr = rdma_buffer_register(sock->pd, &sock->remote_metadata_attr, sizeof(sock->remote_metadata_attr), (IBV_ACCESS_LOCAL_WRITE));
        if (!sock->mr) {
            rdma_error("Failed to setup the server metadata mr , -ENOMEM\n");
            return -ENOMEM;
        }
        struct ibv_sge server_recv_sge;
        struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr;
        server_recv_sge.addr = (uint64_t)sock->mr->addr;
        server_recv_sge.length = (uint32_t)sock->mr->length;
        server_recv_sge.lkey = (uint32_t)sock->mr->lkey;
        /* now we link it to the request */
        bzero(&server_recv_wr, sizeof(server_recv_wr));
        server_recv_wr.sg_list = &server_recv_sge;
        server_recv_wr.num_sge = 1;
        debug("connect: preposting to recv\n");

        ret = ibv_post_recv(sock->qp /* which QP */, &server_recv_wr /* receive work request*/, &bad_server_recv_wr /* error WRs */);
        if (ret) {
            rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
            return ret;
        }
        debug("Receive buffer pre-posting is successful \n");
        return ret;

        return -ENOSYS;
    }
    // the default path
    return _connect(sockfd, addr, addrlen);
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
