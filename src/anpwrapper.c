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
static int (*_listen)(int __fd, int __n) = NULL;


/// server stuff
/* These are the RDMA resources needed to setup an RDMA connection */
/* Event channel, where connection management (cm) related events are relayed */
static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_host_id = NULL, *cm_remote_id = NULL;
static struct ibv_pd *pd = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_cq *cq = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *client_qp = NULL;
/* RDMA memory resources */
static struct ibv_mr *client_metadata_mr = NULL, *server_buffer_mr = NULL, *server_metadata_mr = NULL;
static struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;
static struct ibv_sge client_recv_sge, server_send_sge;



/// client stuff
/* These are basic RDMA resources */
/* These are RDMA connection related resources */
static struct ibv_cq *client_cq = NULL;
/* These are memory buffers related resources */
static struct ibv_mr *client_src_mr = NULL, 
		     *client_dst_mr = NULL;
static struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;
static struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
static struct ibv_sge client_send_sge, server_recv_sge;
/* Source and Destination buffers, where RDMA operations source and sink */
static char *src = NULL, *dst = NULL; 


// stuff
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
		int ret = -1;
		/*  Open a channel used to report asynchronous communication event */
		cm_event_channel = rdma_create_event_channel();
		if (!cm_event_channel) {
			rdma_error("Creating cm event channel failed with errno : (%d)", -errno);
			return -errno;
		}
		debug("RDMA CM event channel is created successfully at %p \n", 
				cm_event_channel);
		/* rdma_cm_id is the connection identifier (like socket) which is used 
		* to define an RDMA connection. 
		*/
		ret = rdma_create_id(cm_event_channel, &cm_host_id, NULL, RDMA_PS_TCP);
		if (ret) {
			rdma_error("Creating cm id failed with errno: %d ", -errno);
			return -errno;
		}
		return ret;

        return -ENOSYS;
    }
    // if this is not what anpnetstack support, let it go, let it go!
    return _socket(domain, type, protocol);
}

int listen(int __fd, int __n) {
	/* Now we start to listen on the passed IP and port. However unlike
	 * normal TCP listen, this is a non-blocking call. When a new client is 
	 * connected, a new connection management (CM) event is generated on the 
	 * RDMA CM event channel from where the listening id was created. Here we
	 * have only one channel, so it is easy. */
	int ret = rdma_listen(cm_host_id, 8); /* backlog = 8 clients, same as TCP, see man listen*/
	if (ret) {
		rdma_error("rdma_listen failed to listen on server address, errno: %d ",
				-errno);
		return -errno;
	}
	// printf("Server is listening successfully at: %s , port: %d \n",
	// 		inet_ntoa(sockaddr->sin_addr), // we don't pass this, todo as fd
	// 		ntohs(sockaddr->sin_port));
	/* now, we expect a client to connect and generate a RDMA_CM_EVNET_CONNECT_REQUEST 
	 * We wait (block) on the connection management event channel for 
	 * the connect event. 
	 */
	ret = process_rdma_cm_event(cm_event_channel, 
			RDMA_CM_EVENT_CONNECT_REQUEST,
			&cm_event);
	if (ret) {
		rdma_error("Failed to get cm event, ret = %d \n" , ret);
		return ret;
	}
	/* Much like TCP connection, listening returns a new connection identifier 
	 * for newly connected client. In the case of RDMA, this is stored in id 
	 * field. For more details: man rdma_get_cm_event 
	 */
	cm_remote_id = cm_event->id;
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
	debug("A new RDMA client connection id is stored at %p\n", cm_remote_id);
	return ret;
}

int accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len) { // TODO: use args instead of ignoring them
	struct rdma_conn_param conn_param;
	struct sockaddr_in remote_sockaddr; 
	int ret = -1;
	if(!cm_remote_id || !client_qp) {
		rdma_error("Client resources are not properly setup\n");
		return -EINVAL;
	}
	/* we prepare the receive buffer in which we will receive the client metadata*/
        client_metadata_mr = rdma_buffer_register(pd /* which protection domain */, 
			&client_metadata_attr /* what memory */,
			sizeof(client_metadata_attr) /* what length */, 
		       (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
	if(!client_metadata_mr){
		rdma_error("Failed to register client attr buffer\n");
		//we assume ENOMEM
		return -ENOMEM;
	}
	/* We pre-post this receive buffer on the QP. SGE credentials is where we 
	 * receive the metadata from the client */
	client_recv_sge.addr = (uint64_t) client_metadata_mr->addr; // same as &client_buffer_attr
	client_recv_sge.length = client_metadata_mr->length;
	client_recv_sge.lkey = client_metadata_mr->lkey;
	/* Now we link this SGE to the work request (WR) */
	bzero(&client_recv_wr, sizeof(client_recv_wr));
	client_recv_wr.sg_list = &client_recv_sge;
	client_recv_wr.num_sge = 1; // only one SGE
	ret = ibv_post_recv(client_qp /* which QP */,
		      &client_recv_wr /* receive work request*/,
		      &bad_client_recv_wr /* error WRs */);
	if (ret) {
		rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
		return ret;
	}
	debug("Receive buffer pre-posting is successful \n");
	/* Now we accept the connection. Recall we have not accepted the connection 
	 * yet because we have to do lots of resource pre-allocation */
       memset(&conn_param, 0, sizeof(conn_param));
       /* this tell how many outstanding requests can we handle */
       conn_param.initiator_depth = 3; /* For this exercise, we put a small number here */
       /* This tell how many outstanding requests we expect other side to handle */
       conn_param.responder_resources = 3; /* For this exercise, we put a small number */
       ret = rdma_accept(cm_remote_id, &conn_param);
       if (ret) {
	       rdma_error("Failed to accept the connection, errno: %d \n", -errno);
	       return -errno;
       }
       /* We expect an RDMA_CM_EVNET_ESTABLISHED to indicate that the RDMA  
	* connection has been established and everything is fine on both, server 
	* as well as the client sides.
	*/
        debug("Going to wait for : RDMA_CM_EVENT_ESTABLISHED event \n");
       ret = process_rdma_cm_event(cm_event_channel, 
		       RDMA_CM_EVENT_ESTABLISHED,
		       &cm_event);
        if (ret) {
		rdma_error("Failed to get the cm event, errnp: %d \n", -errno);
		return -errno;
	}
	/* We acknowledge the event */
	ret = rdma_ack_cm_event(cm_event);
	if (ret) {
		rdma_error("Failed to acknowledge the cm event %d\n", -errno);
		return -errno;
	}
	return ret;
}

int bind(int socket, const struct sockaddr *server_sockaddr, socklen_t address_len) { // address_len is useless here because of RDMA
	int ret = rdma_bind_addr(cm_host_id, (struct sockaddr*) server_sockaddr);
	if (ret) {
		rdma_error("Failed to bind server address, errno: %d \n", -errno);
		return -errno;
	}
	return ret;
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
		if (cm_host_id == NULL) rdma_disconnect(cm_host_id); // hacky way to check if we are server or client
		process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_DISCONNECTED,
									&cm_event);
		rdma_ack_cm_event(cm_event);
		/* Destroy QP */
		rdma_destroy_qp(cm_host_id);
		/* Destroy client cm id */
		rdma_destroy_id(cm_host_id);
		if (cm_remote_id != NULL) rdma_destroy_id(cm_remote_id);
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
		printf("cm host id: %i, cm remote id: %i\n", cm_host_id, cm_remote_id);
    if (is_anp_sockfd) {

		int ret = rdma_resolve_addr(cm_host_id, NULL, (struct sockaddr*) addr, 2000);
		if (ret) {
			rdma_error("Failed to resolve address, errno: %d \n", -errno);
			return -errno;
		}
		printf("something\n");
		debug("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n");
		ret  = process_rdma_cm_event(cm_event_channel, 
				RDMA_CM_EVENT_ADDR_RESOLVED,
				&cm_event);
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
		ret = rdma_resolve_route(cm_host_id, 2000);
		if (ret) {
			rdma_error("Failed to resolve route, erno: %d \n", -errno);
			return -errno;
		}
		debug("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
		ret = process_rdma_cm_event(cm_event_channel, 
				RDMA_CM_EVENT_ROUTE_RESOLVED,
				&cm_event);
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
		// printf("Trying to connect to server at : %s port: %d \n", 
		// 		inet_ntoa(sockaddr->sin_addr),
		// 		ntohs(sockaddr->sin_port));
		/* Protection Domain (PD) is similar to a "process abstraction" 
		* in the operating system. All resources are tied to a particular PD. 
		* And accessing recourses across PD will result in a protection fault.
		*/
		pd = ibv_alloc_pd(cm_host_id->verbs);
		if (!pd) {
			rdma_error("Failed to alloc pd, errno: %d \n", -errno);
			return -errno;
		}
		debug("pd allocated at %p \n", pd);
		/* Now we need a completion channel, were the I/O completion 
		* notifications are sent. Remember, this is different from connection 
		* management (CM) event notifications. 
		* A completion channel is also tied to an RDMA device, hence we will 
		* use cm_host_id->verbs. 
		*/
		io_completion_channel = ibv_create_comp_channel(cm_host_id->verbs);
		if (!io_completion_channel) {
			rdma_error("Failed to create IO completion event channel, errno: %d\n",
					-errno);
		return -errno;
		}
		debug("completion event channel created at : %p \n", io_completion_channel);
		/* Now we create a completion queue (CQ) where actual I/O 
		* completion metadata is placed. The metadata is packed into a structure 
		* called struct ibv_wc (wc = work completion). ibv_wc has detailed 
		* information about the work completion. An I/O request in RDMA world 
		* is called "work" ;) 
		*/
		client_cq = ibv_create_cq(cm_host_id->verbs /* which device*/, 
				CQ_CAPACITY /* maximum capacity*/, 
				NULL /* user context, not used here */,
				io_completion_channel /* which IO completion channel */, 
				0 /* signaling vector, not used here*/);
		if (!client_cq) {
			rdma_error("Failed to create CQ, errno: %d \n", -errno);
			return -errno;
		}
		debug("CQ created at %p with %d elements \n", client_cq, client_cq->cqe);
		ret = ibv_req_notify_cq(client_cq, 0);
		if (ret) {
			rdma_error("Failed to request notifications, errno: %d\n", -errno);
			return -errno;
		}
		/* Now the last step, set up the queue pair (send, recv) queues and their capacity.
			* The capacity here is define statically but this can be probed from the 
		* device. We just use a small number as defined in rdma_common.h */
		bzero(&qp_init_attr, sizeof qp_init_attr);
		qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
		qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
		qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
		qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
		qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
		/* We use same completion queue, but one can use different queues */
		qp_init_attr.recv_cq = client_cq; /* Where should I notify for receive completion operations */
		qp_init_attr.send_cq = client_cq; /* Where should I notify for send completion operations */
		/*Lets create a QP */
		ret = rdma_create_qp(cm_host_id /* which connection id */,
				pd /* which protection domain*/,
				&qp_init_attr /* Initial attributes */);
		if (ret) {
			rdma_error("Failed to create QP, errno: %d \n", -errno);
			return -errno;
		}
		client_qp = cm_host_id->qp;
		debug("QP created at %p \n", client_qp);

		struct rdma_conn_param conn_param;
		ret = -1;
		bzero(&conn_param, sizeof(conn_param));
		conn_param.initiator_depth = 3;
		conn_param.responder_resources = 3;
		conn_param.retry_count = 3; // if fail, then how many times to retry
		ret = rdma_connect(cm_host_id, &conn_param);
		if (ret) {
			rdma_error("Failed to connect to remote host , errno: %d\n", -errno);
			return -errno;
		}
		debug("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED\n");
		ret = process_rdma_cm_event(cm_event_channel, 
				RDMA_CM_EVENT_ESTABLISHED,
				&cm_event);
		if (ret) {
			rdma_error("Failed to get cm event, ret = %d \n", ret);
			return ret;
		}
		ret = rdma_ack_cm_event(cm_event);
		if (ret) {
			rdma_error("Failed to acknowledge cm event, errno: %d\n", 
					-errno);
			return -errno;
		}
		printf("The client is connected successfully \n");
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
}
