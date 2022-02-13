/*
 * This is a RDMA server side code.
 *
 * Author: Animesh Trivedi
 *         atrivedi@apache.org
 *
 * TODO: Cleanup previously allocated resources in case of an error condition
 */

#include "rdma_common.h"

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

/* Starts an RDMA server by allocating basic connection resources */
static int start_rdma_server(struct sockaddr_in *server_sockaddr) {
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;
    cm_event_channel = rdma_create_event_channel();
    ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
    ret = rdma_bind_addr(cm_server_id, (struct sockaddr *)server_sockaddr);
    ret = rdma_listen(cm_server_id, 8); /* backlog = 8 clients, same as TCP, see man listen*/
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_CONNECT_REQUEST, &cm_event);
    cm_client_id = cm_event->id;
    ret = rdma_ack_cm_event(cm_event);
    return ret;
}

/* When we call this function cm_client_id must be set to a valid identifier.
 * This is where, we prepare client connection before we accept it. This
 * mainly involve pre-posting a receive buffer to receive client side
 * RDMA credentials
 */
static int setup_client_resources() {
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
    ret = rdma_create_qp(cm_client_id /* which connection id */,
                         pd /* which protection domain*/,
                         &qp_init_attr /* Initial attributes */);
    /* Save the reference for handy typing but is not required */
    client_qp = cm_client_id->qp;
    return ret;
}

/* Pre-posts a receive buffer and accepts an RDMA client connection */
static int accept_client_connection() {
    struct rdma_conn_param conn_param;
    struct rdma_cm_event *cm_event = NULL;
    struct sockaddr_in remote_sockaddr;
    int ret = -1;
    /* we prepare the receive buffer in which we will receive the client
     * metadata*/
    client_metadata_mr =
        rdma_buffer_register(pd /* which protection domain */,
                             &client_metadata_attr /* what memory */,
                             sizeof(client_metadata_attr) /* what length */,
                             (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
    /* We pre-post this receive buffer on the QP. SGE credentials is where we
     * receive the metadata from the client */
    client_recv_sge.addr =
        (uint64_t)client_metadata_mr->addr;  // same as &client_buffer_attr
    client_recv_sge.length = client_metadata_mr->length;
    client_recv_sge.lkey = client_metadata_mr->lkey;
    /* Now we link this SGE to the work request (WR) */
    bzero(&client_recv_wr, sizeof(client_recv_wr));
    client_recv_wr.sg_list = &client_recv_sge;
    client_recv_wr.num_sge = 1;  // only one SGE
    ret = ibv_post_recv(client_qp /* which QP */,
                        &client_recv_wr /* receive work request*/,
                        &bad_client_recv_wr /* error WRs */);
    /* Now we accept the connection. Recall we have not accepted the connection
     * yet because we have to do lots of resource pre-allocation */
    memset(&conn_param, 0, sizeof(conn_param));
    /* this tell how many outstanding requests can we handle */
    conn_param.initiator_depth =
        3; /* For this exercise, we put a small number here */
    /* This tell how many outstanding requests we expect other side to handle */
    conn_param.responder_resources =
        3; /* For this exercise, we put a small number */
    ret = rdma_accept(cm_client_id, &conn_param);
    /* We expect an RDMA_CM_EVNET_ESTABLISHED to indicate that the RDMA
     * connection has been established and everything is fine on both, server
     * as well as the client sides.
     */
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED,
                                &cm_event);
    /* We acknowledge the event */
    ret = rdma_ack_cm_event(cm_event);
    /* Just FYI: How to extract connection information */
    memcpy(&remote_sockaddr /* where to save */,
           rdma_get_peer_addr(cm_client_id) /* gives you remote sockaddr */,
           sizeof(struct sockaddr_in) /* max size */);
    return ret;
}

/* This function sends server side buffer metadata to the connected client */
static int send_server_metadata_to_client() {
    struct ibv_wc wc;
    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    show_rdma_buffer_attr(&client_metadata_attr);
    server_buffer_mr = rdma_buffer_alloc(pd /* which protection domain */, client_metadata_attr.length /* what size to allocate */, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE) /* access permissions */);
    server_metadata_attr.address = (uint64_t)server_buffer_mr->addr;
    server_metadata_attr.length = (uint32_t)server_buffer_mr->length;
    server_metadata_attr.stag.local_stag = (uint32_t)server_buffer_mr->lkey;
    server_metadata_mr = rdma_buffer_register(pd /* which protection domain*/, &server_metadata_attr /* which memory to register */, sizeof(server_metadata_attr) /* what is the size of memory */, IBV_ACCESS_LOCAL_WRITE /* what access permission */);
    server_send_sge.addr = (uint64_t)&server_metadata_attr;
    server_send_sge.length = sizeof(server_metadata_attr);
    server_send_sge.lkey = server_metadata_mr->lkey;
    bzero(&server_send_wr, sizeof(server_send_wr));
    server_send_wr.sg_list = &server_send_sge;
    server_send_wr.num_sge = 1;           // only 1 SGE element in the array
    server_send_wr.opcode = IBV_WR_SEND;  // This is a send request
    server_send_wr.send_flags = IBV_SEND_SIGNALED;  // We want to get notification
    ret = ibv_post_send(client_qp /* which QP */, &server_send_wr /* Send request that we prepared before */, &bad_server_send_wr /* In case of error, this will contain failed requests */);
    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    return 0;
}

/* This is server side logic. Server passively waits for the client to call
 * rdma_disconnect() and then it will clean up its resources */
static int disconnect_and_cleanup() {
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;
    /* Now we wait for the client to send us disconnect event */
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_DISCONNECTED,
                                &cm_event);
    /* We acknowledge the event */
    ret = rdma_ack_cm_event(cm_event);
    /* We free all the resources */
    /* Destroy QP */
    rdma_destroy_qp(cm_client_id);
    /* Destroy client cm id */
    ret = rdma_destroy_id(cm_client_id);
    /* Destroy CQ */
    ret = ibv_destroy_cq(cq);
    /* Destroy completion channel */
    ret = ibv_destroy_comp_channel(io_completion_channel);
    /* Destroy memory buffers */
    rdma_buffer_free(server_buffer_mr);
    rdma_buffer_deregister(server_metadata_mr);
    rdma_buffer_deregister(client_metadata_mr);
    /* Destroy protection domain */
    ret = ibv_dealloc_pd(pd);
    /* Destroy rdma server id */
    ret = rdma_destroy_id(cm_server_id);
    rdma_destroy_event_channel(cm_event_channel);
    return 0;
}

void usage() {
    printf("Usage:\n");
    printf("rdma_server: [-a <server_addr>] [-p <server_port>]\n");
    printf("(default port is %d)\n", DEFAULT_RDMA_PORT);
    exit(1);
}

int main(int argc, char **argv) {
    int ret, option;
    struct sockaddr_in server_sockaddr;
    bzero(&server_sockaddr, sizeof server_sockaddr);
    server_sockaddr.sin_family = AF_INET; /* standard IP NET address */
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY); /* passed address */
    /* Parse Command Line Arguments, not the most reliable code */
    while ((option = getopt(argc, argv, "a:p:")) != -1) {
        switch (option) {
            case 'a':
                /* Remember, this will overwrite the port info */
                ret = get_addr(optarg, (struct sockaddr *)&server_sockaddr);
                if (ret) {
                    rdma_error("Invalid IP \n");
                    return ret;
                }
                break;
            case 'p':
                /* passed port to listen on */
                server_sockaddr.sin_port = htons(strtol(optarg, NULL, 0));
                break;
            default:
                usage();
                break;
        }
    }
    if (!server_sockaddr.sin_port) {
        /* If still zero, that mean no port info provided */
        server_sockaddr.sin_port =
            htons(DEFAULT_RDMA_PORT); /* use default port */
    }
        struct rdma_cm_event *cm_event = NULL;

	int ret = -1;
    
	// START_RDMA_SERVER
    cm_event_channel = rdma_create_event_channel();
    ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
    ret = rdma_bind_addr(cm_server_id, (struct sockaddr *)server_sockaddr);
    ret = rdma_listen(cm_server_id, 8); /* backlog = 8 clients, same as TCP, see man listen*/
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_CONNECT_REQUEST, &cm_event);
    cm_client_id = cm_event->id;
    ret = rdma_ack_cm_event(cm_event);

	//SETUP_CLIENT_RESOURCES
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


    /* Just FYI: How to extract connection information */
    memcpy(&remote_sockaddr /* where to save */, rdma_get_peer_addr(cm_client_id) /* gives you remote sockaddr */, sizeof(struct sockaddr_in) /* max size */);

	// SEND_SERVER_METADATA_TO_CLIENT
	struct ibv_wc wc;
    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    show_rdma_buffer_attr(&client_metadata_attr);
    server_buffer_mr = rdma_buffer_alloc(pd /* which protection domain */, client_metadata_attr.length /* what size to allocate */, (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE) /* access permissions */);
    server_metadata_attr.address = (uint64_t)server_buffer_mr->addr;
    server_metadata_attr.length = (uint32_t)server_buffer_mr->length;
    server_metadata_attr.stag.local_stag = (uint32_t)server_buffer_mr->lkey;
    server_metadata_mr = rdma_buffer_register(pd /* which protection domain*/, &server_metadata_attr /* which memory to register */, sizeof(server_metadata_attr) /* what is the size of memory */, IBV_ACCESS_LOCAL_WRITE /* what access permission */);
    server_send_sge.addr = (uint64_t)&server_metadata_attr;
    server_send_sge.length = sizeof(server_metadata_attr);
    server_send_sge.lkey = server_metadata_mr->lkey;
    bzero(&server_send_wr, sizeof(server_send_wr));
    server_send_wr.sg_list = &server_send_sge;
    server_send_wr.num_sge = 1;           // only 1 SGE element in the array
    server_send_wr.opcode = IBV_WR_SEND;  // This is a send request
    server_send_wr.send_flags = IBV_SEND_SIGNALED;  // We want to get notification
    ret = ibv_post_send(client_qp /* which QP */, &server_send_wr /* Send request that we prepared before */, &bad_server_send_wr /* In case of error, this will contain failed requests */);
    ret = process_work_completion_events(io_completion_channel, &wc, 1);

	// DISCONNECT AND CLEANUP
    ret = disconnect_and_cleanup();
    return 0;
}
