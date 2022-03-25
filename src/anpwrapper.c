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

#include "init.h"
#include "linklist.h"
#include "rdma_common.h"
#include "systems_headers.h"

static int (*__start_main)(int (*main)(int, char **, char **), int argc,
                           char **ubp_av, void (*init)(void),
                           void (*fini)(void), void (*rtld_fini)(void),
                           void(*stack_end));

static int (*_send)(int fd, const void *buf, size_t n, int flags) = NULL;
static int (*_recv)(int fd, void *buf, size_t n, int flags) = NULL;

static int (*_connect)(int sockfd, const struct sockaddr *addr,
                       socklen_t addrlen) = NULL;
static int (*_socket)(int domain, int type, int protocol) = NULL;
static int (*_close)(int sockfd) = NULL;

static int (*_accept)(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len) = NULL;
static int (*_bind)(int socket, const struct sockaddr *server_sockaddr, socklen_t address_len) = NULL;
static int (*_listen)(int socket, int backlog) = NULL;


// Copied from server
/* These are the RDMA resources needed to setup an RDMA connection */
/* Event channel, where connection management (cm) related events are relayed */
static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_server_id = NULL, *cm_client_id = NULL;
static struct ibv_pd *pd = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_cq *cq = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *client_qp = NULL;
/* RDMA memory resources */
static struct ibv_mr *client_metadata_mr = NULL, *server_buffer_mr = NULL,
                     *server_metadata_mr = NULL;
static struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;
static struct ibv_sge client_recv_sge, server_send_sge;


// Copied from client
/* These are basic RDMA resources */
/* These are RDMA connection related resources */
static struct ibv_cq *client_cq = NULL;
/* These are memory buffers related resources */
static struct ibv_mr *client_src_mr = NULL, *client_dst_mr = NULL;
static struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;
static struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
static struct ibv_sge client_send_sge, server_recv_sge;
/* Source and Destination buffers, where RDMA operations source and sink */
static char *src = NULL, *dst = NULL;
struct rdma_cm_event *cm_event = NULL;


static int is_socket_supported(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        return 0;
    }
    if (!(type & SOCK_STREAM)) {
        return 0;
    }
    if (protocol != 0 && protocol != IPPROTO_TCP) {
        return 0;
    }
    printf("supported socket domain %d type %d and protocol %d \n", domain,
           type, protocol);
    return 1;
}

int socket(int domain, int type, int protocol) {
    if (is_socket_supported(domain, type, protocol)) {
        /*  Open a channel used to report asynchronous communication event */
        cm_event_channel = rdma_create_event_channel();
        /* rdma_cm_id is the connection identifier (like socket) which is used
         * to define an RDMA connection.
         */
        return rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);

        return -ENOSYS;
    }
    // if this is not what anpnetstack support, let it go, let it go!
    return _socket(domain, type, protocol);
}

int listen(int socket, int backlog) { //TODO: this also connects, but we should connect in accept()
	int ret = -1;
    /* Now we start to listen on the passed IP and port. However unlike
     * normal TCP listen, this is a non-blocking call. When a new client is
     * connected, a new connection management (CM) event is generated on the
     * RDMA CM event channel from where the listening id was created. Here
     * we have only one channel, so it is easy. */
    ret = rdma_listen(cm_server_id, 8); /* backlog = 8 clients, same as TCP, see man listen*/
    
    /* now, we expect a client to connect and generate a
     * RDMA_CM_EVNET_CONNECT_REQUEST We wait (block) on the connection
     * management event channel for the connect event.
     */
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_CONNECT_REQUEST, &cm_event);
    
    /* Much like TCP connection, listening returns a new connection
     * identifier for newly connected client. In the case of RDMA, this is
     * stored in id field. For more details: man rdma_get_cm_event
     */
    cm_client_id = cm_event->id;
    /* now we acknowledge the event. Acknowledging the event free the
     * resources associated with the event structure. Hence any reference to
     * the event must be made before acknowledgment. Like, we have already
     * saved the client id from "id" field before acknowledging the event.
     */
    ret = rdma_ack_cm_event(cm_event);
    return ret;
}

int accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len) { // TODO: use args instead of ignoring them

	//SETUP_CLIENT_RESOURCES
	int ret = -1;
	pd = ibv_alloc_pd(cm_client_id->verbs);
    io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
    cq = ibv_create_cq(cm_client_id->verbs /* which device*/,
                       CQ_CAPACITY /* maximum capacity*/,
                       NULL /* user context, not used here */,
                       io_completion_channel /* which IO completion channel */,
                       0 /* signaling vector, not used here*/);
    ret = ibv_req_notify_cq(cq /* on which CQ */,
                            0 /* 0 = all event type, no filter*/);
    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
    qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
    qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
    qp_init_attr.cap.max_send_wr = MAX_WR;   /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
    qp_init_attr.recv_cq = cq; /* Where should I notify for receive completion operations */
    qp_init_attr.send_cq = cq; /* Where should I notify for send completion operations */
    ret = rdma_create_qp(cm_client_id /* which connection id */, pd /* which protection domain*/, &qp_init_attr /* Initial attributes */);
    /* Save the reference for handy typing but is not required */
    client_qp = cm_client_id->qp;


	// ACCEPT_CLIENT_CONNECTION
    struct rdma_conn_param conn_param;
    struct rdma_cm_event *cm_event = NULL;
    struct sockaddr_in remote_sockaddr;
    /* we prepare the receive buffer in which we will receive the client
     * metadata*/
    client_metadata_mr = rdma_buffer_register(	pd /* which protection domain */, &client_metadata_attr /* what memory */, sizeof(client_metadata_attr) /* what length */, (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
    /* We pre-post this receive buffer on the QP. SGE credentials is where we
     * receive the metadata from the client */
    client_recv_sge.addr = (uint64_t)client_metadata_mr->addr;  // same as &client_buffer_attr
    client_recv_sge.length = client_metadata_mr->length;
    client_recv_sge.lkey = client_metadata_mr->lkey;
    /* Now we link this SGE to the work request (WR) */
    bzero(&client_recv_wr, sizeof(client_recv_wr));
    client_recv_wr.sg_list = &client_recv_sge;
    client_recv_wr.num_sge = 1;  // only one SGE
    ret = ibv_post_recv(client_qp /* which QP */, &client_recv_wr /* receive work request*/, &bad_client_recv_wr /* error WRs */);
    /* Now we accept the connection. Recall we have not accepted the connection
     * yet because we have to do lots of resource pre-allocation */
    memset(&conn_param, 0, sizeof(conn_param));
    /* this tell how many outstanding requests can we handle */
    conn_param.initiator_depth = 3; /* For this exercise, we put a small number here */
    /* This tell how many outstanding requests we expect other side to handle */
    conn_param.responder_resources = 3; /* For this exercise, we put a small number */
    ret = rdma_accept(cm_client_id, &conn_param);
    /* We expect an RDMA_CM_EVNET_ESTABLISHED to indicate that the RDMA
     * connection has been established and everything is fine on both, server
     * as well as the client sides.
     */
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
    /* We acknowledge the event */
    ret = rdma_ack_cm_event(cm_event);
	// CONNECTION ESTABLISHED WHEN WE GET RDMA_CM_EVNET_ESTABLISHED

	// FYI how to get connection info
    memcpy(&remote_sockaddr /* where to save */, rdma_get_peer_addr(cm_client_id) /* gives you remote sockaddr */, sizeof(struct sockaddr_in) /* max size */);
}

int bind(int socket, const struct sockaddr *server_sockaddr, socklen_t address_len) { // address_len is useless here because of RDMA
    /* Explicit binding of rdma cm id to the socket credentials */
	int ret = -1;
    ret = rdma_bind_addr(cm_server_id, (struct sockaddr*) server_sockaddr);
    debug("Server RDMA CM id is successfully binded \n");
}


ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    // FIXME -- you can remember the file descriptors that you have generated in
    // the socket call and match them here
    bool is_anp_sockfd = false;
    if (is_anp_sockfd) {
        

		//rdma_send:
		printf("rdma_send");


        return -ENOSYS;
    }
    // the default path
    return _send(sockfd, buf, len, flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    // FIXME -- you can remember the file descriptors that you have generated in
    // the socket call and match them here
    bool is_anp_sockfd = false;
    if (is_anp_sockfd) {
        
		
		//rdma_recv:
		
		printf("rdma_recv");


        return -ENOSYS;
    }
    // the default path
    return _recv(sockfd, buf, len, flags);
}

int close(int sockfd) {
    // FIXME -- you can remember the file descriptors that you have generated in
    // the socket call and match them here
    bool is_anp_sockfd = false;
    if (is_anp_sockfd) {
		// close code
		if (cm_server_id == NULL) rdma_disconnect(cm_client_id); // hacky way to check if we are server or client
		process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_DISCONNECTED,
									&cm_event);
		rdma_ack_cm_event(cm_event);
		/* Destroy QP */
		rdma_destroy_qp(cm_client_id);
		/* Destroy client cm id */
		rdma_destroy_id(cm_client_id);
		if (cm_server_id != NULL) rdma_destroy_id(cm_server_id);
		/* Destroy CQ */
		ibv_destroy_cq(client_cq);
		/* Destroy completion channel */
		ibv_destroy_comp_channel(io_completion_channel);
		/* Destroy memory buffers */
		if (server_metadata_mr != NULL) rdma_buffer_deregister(server_metadata_mr);
		if (client_metadata_mr != NULL) rdma_buffer_deregister(client_metadata_mr);
		if (client_src_mr != NULL) rdma_buffer_deregister(client_src_mr);
		if (client_dst_mr != NULL) rdma_buffer_deregister(client_dst_mr);

		if (server_buffer_mr != NULL) rdma_buffer_free(server_buffer_mr);

		/* We free the buffers */
		if (src != NULL) free(src);
		if (dst != NULL) free(dst);
		/* Destroy protection domain */
		ibv_dealloc_pd(pd);
		rdma_destroy_event_channel(cm_event_channel);
		printf("Resource clean up is complete \n");
		return 0;
    }
    // the default path
    return _close(sockfd);
}


int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    // FIXME -- you can remember the file descriptors that you have generated in
    // the socket call and match them here
    bool is_anp_sockfd = false;
    if (is_anp_sockfd) {

		// connect start
		rdma_resolve_addr(cm_client_id, NULL, (struct sockaddr *) addr, 2000);
		process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ADDR_RESOLVED,
									&cm_event);
		/* we ack the event */
		rdma_ack_cm_event(cm_event);

		/* Resolves an RDMA route to the destination address in order to
		* establish a connection */
		rdma_resolve_route(cm_client_id, 2000);
		process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ROUTE_RESOLVED,
									&cm_event);
		/* we ack the event */
		rdma_ack_cm_event(cm_event);

		/* Protection Domain (PD) is similar to a "process abstraction"
		* in the operating system. All resources are tied to a particular PD.
		* And accessing recourses across PD will result in a protection fault.
		*/
		pd = ibv_alloc_pd(cm_client_id->verbs);
		/* Now we need a completion channel, were the I/O completion
		* notifications are sent. Remember, this is different from connection
		* management (CM) event notifications.
		* A completion channel is also tied to an RDMA device, hence we will
		* use cm_client_id->verbs.
		*/
		io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
		/* Now we create a completion queue (CQ) where actual I/O
		* completion metadata is placed. The metadata is packed into a structure
		* called struct ibv_wc (wc = work completion). ibv_wc has detailed
		* information about the work completion. An I/O request in RDMA world
		* is called "work" ;)
		*/
		client_cq =
			ibv_create_cq(cm_client_id->verbs /* which device*/,
						CQ_CAPACITY /* maximum capacity*/,
						NULL /* user context, not used here */,
						io_completion_channel /* which IO completion channel */,
						0 /* signaling vector, not used here*/);
		ibv_req_notify_cq(client_cq, 0);
		/* Now the last step, set up the queue pair (send, recv) queues and their
		* capacity. The capacity here is define statically but this can be probed
		* from the device. We just use a small number as defined in rdma_common.h
		*/
		bzero(&qp_init_attr, sizeof qp_init_attr);
		qp_init_attr.cap.max_recv_sge =
			MAX_SGE; /* Maximum SGE per receive posting */
		qp_init_attr.cap.max_recv_wr =
			MAX_WR; /* Maximum receive posting capacity */
		qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
		qp_init_attr.cap.max_send_wr = MAX_WR;   /* Maximum send posting capacity */
		qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
		/* We use same completion queue, but one can use different queues */
		qp_init_attr.recv_cq =
			client_cq; /* Where should I notify for receive completion operations */
		qp_init_attr.send_cq =
			client_cq; /* Where should I notify for send completion operations */
		/*Lets create a QP */
		rdma_create_qp(cm_client_id /* which connection id */,
							pd /* which protection domain*/,
							&qp_init_attr /* Initial attributes */);
		client_qp = cm_client_id->qp;




		// CLIENT_PRE_POST_RECV_BUFFER
		server_metadata_mr = rdma_buffer_register(pd, &server_metadata_attr,
												sizeof(server_metadata_attr),
												(IBV_ACCESS_LOCAL_WRITE));
		server_recv_sge.addr = (uint64_t)server_metadata_mr->addr;
		server_recv_sge.length = (uint32_t)server_metadata_mr->length;
		server_recv_sge.lkey = (uint32_t)server_metadata_mr->lkey;
		/* now we link it to the request */
		bzero(&server_recv_wr, sizeof(server_recv_wr));
		server_recv_wr.sg_list = &server_recv_sge;
		server_recv_wr.num_sge = 1;
		ibv_post_recv(client_qp /* which QP */,
							&server_recv_wr /* receive work request*/,
							&bad_server_recv_wr /* error WRs */);
		debug("Receive buffer pre-posting is successful \n");


		// RDMA_CLIENT_CONNECT_TO_SERVER
		struct rdma_conn_param conn_param;
		struct rdma_cm_event *cm_event = NULL;
		bzero(&conn_param, sizeof(conn_param));
		conn_param.initiator_depth = 3;
		conn_param.responder_resources = 3;
		conn_param.retry_count = 3;  // if fail, then how many times to retry
		rdma_connect(cm_client_id, &conn_param);
		debug("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED\n");
		process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED,
									&cm_event);
		rdma_ack_cm_event(cm_event);
		printf("The client is connected successfully \n");

		// CLIENT_EXCHANGE_METADATA_WITH_SERVER
		struct ibv_wc wc[2];
		client_src_mr =
			rdma_buffer_register(pd, src, strlen(src),
								(IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
								IBV_ACCESS_REMOTE_WRITE));
		/* we prepare metadata for the first buffer */
		client_metadata_attr.address = (uint64_t)client_src_mr->addr;
		client_metadata_attr.length = client_src_mr->length;
		client_metadata_attr.stag.local_stag = client_src_mr->lkey;
		/* now we register the metadata memory */
		client_metadata_mr = rdma_buffer_register(pd, &client_metadata_attr,
												sizeof(client_metadata_attr),
												IBV_ACCESS_LOCAL_WRITE);
		/* now we fill up SGE */
		client_send_sge.addr = (uint64_t)client_metadata_mr->addr;
		client_send_sge.length = (uint32_t)client_metadata_mr->length;
		client_send_sge.lkey = client_metadata_mr->lkey;
		/* now we link to the send work request */
		bzero(&client_send_wr, sizeof(client_send_wr));
		client_send_wr.sg_list = &client_send_sge;
		client_send_wr.num_sge = 1;
		client_send_wr.opcode = IBV_WR_SEND;
		client_send_wr.send_flags = IBV_SEND_SIGNALED;
		/* Now we post it */
		ibv_post_send(client_qp, &client_send_wr, &bad_client_send_wr);
		/* at this point we are expecting 2 work completion. One for our
		* send and one for recv that we will get from the server for
		* its buffer information */
		process_work_completion_events(io_completion_channel, wc, 2);
		debug("Server sent us its buffer location and credentials, showing \n");
		show_rdma_buffer_attr(&server_metadata_attr);

		return 0;

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
}
