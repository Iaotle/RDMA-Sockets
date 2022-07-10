/*
 * Header file for the common RDMA routines used in the server/client example
 * program.
 *
 * Author: Animesh Trivedi
 *          atrivedi@apache.org
 * Modified by Vadim Isakov
 *
 */

#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util.h"
/* Error Macro*/
#define rdma_error(msg, args...)                                               \
    do {                                                                       \
        fprintf(stderr, "%s : %d : ERROR : " msg, __FILE__, __LINE__, ##args); \
    } while (0);

#ifdef ACN_RDMA_DEBUG
/* Debug Macro */
#define debug(msg, args...)            \
    do {                               \
        printf("DEBUG: " msg, ##args); \
    } while (0);

#else

#define debug(msg, args...)

#endif /* ACN_RDMA_DEBUG */

/* Capacity of the completion queue (CQ) */
#define CQ_CAPACITY (16351)
/* MAX SGE capacity */
#define MAX_SGE (8)
/* MAX work requests */
#define MAX_WR (16351)
/* Default port where the RDMA server is listening */
#define DEFAULT_RDMA_PORT (20886)

#define GC_NUM 10000  // number of buffers to garbagecollect after

#define RECVBUF_SIZE GIGABYTE // size of the receive buffer 


/*
 * We use attribute so that compiler does not step in and try to pad the structure.
 * We use this structure to exchange information between the server and the client.
 *
 * For details see: http://gcc.gnu.org/onlinedocs/gcc/Type-Attributes.html
 */
struct __attribute((packed)) rdma_buffer_attr {
    uint64_t address;
    uint32_t length;
    union stag {
        /* if we send, we call it local stags */
        uint32_t local_stag;
        /* if we receive, we call it remote stag */
        uint32_t remote_stag;
    } stag;
};


typedef struct sock {
    /* These are the RDMA resources needed to setup an RDMA connection */
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
    struct ibv_mr *metadata_mr, *metadata_mr2, *mr_recv, *mr_send;

    /* Optimization in order to not re-register buffers */
    const void *last_bufptr_send;
    size_t last_len_send;

    const void *last_bufptr_recv;
    size_t last_len_recv;

    struct ibv_mr *gc_container[GC_NUM];
    int gc_counter;


	// dedicated buffers:
    struct ibv_mr *recv_buf;  // RECVBUF_SIZE buffer for receiving messages (512mb default?)
	struct rdma_buffer_attr remote_recvbuf; // remote recv_buf to send messages to
	int remote_written; // how much we wrote to remote buffer
	int local_read; // how much was read from the buffer
	int local_written; // how much was written to the buffer

} sock;

void gc_sock(sock *sock);

/* resolves a given destination name to sin_addr */
int get_addr(char *dst, struct sockaddr *addr);

/* prints RDMA buffer info structure */
void show_rdma_buffer_attr(struct rdma_buffer_attr *attr);

/*
 * Processes an RDMA connection management (CM) event.
 * @echannel: CM event channel where the event is expected.
 * @expected_event: Expected event type
 * @cm_event: where the event will be stored
 */
int process_rdma_cm_event(struct rdma_event_channel *echannel, enum rdma_cm_event_type expected_event, struct rdma_cm_event **cm_event);

/* Allocates an RDMA buffer of size 'length' with permission permission. This
 * function will also register the memory and returns a memory region (MR)
 * identifier or NULL on error.
 * @pd: Protection domain where the buffer should be allocated
 * @length: Length of the buffer
 * @permission: OR of IBV_ACCESS_* permissions as defined for the enum ibv_access_flags
 */
struct ibv_mr *rdma_buffer_alloc(struct ibv_pd *pd, uint32_t length, enum ibv_access_flags permission, void *buf);

/* Frees a previously allocated RDMA buffer. The buffer must be allocated by
 * calling rdma_buffer_alloc();
 * @mr: RDMA memory region to free
 */
void rdma_buffer_free(struct ibv_mr *mr);

/* This function registers a previously allocated memory. Returns a memory region
 * (MR) identifier or NULL on error.
 * @pd: protection domain where to register memory
 * @addr: Buffer address
 * @length: Length of the buffer
 * @permission: OR of IBV_ACCESS_* permissions as defined for the enum ibv_access_flags
 */
struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, const void *addr, uint32_t length, enum ibv_access_flags permission);

/* Deregisters a previously register memory
 * @mr: Memory region to deregister
 */
void rdma_buffer_deregister(struct ibv_mr *mr);

/* Processes a work completion (WC) notification.
 * @comp_channel: Completion channel where the notifications are expected to arrive
 * @wc: Array where to hold the work completion elements
 * @max_wc: Maximum number of expected work completion (WC) elements. wc must be
 *          atleast this size.
 */
int process_work_completion_events(struct ibv_comp_channel *comp_channel, struct ibv_wc *wc, int max_wc);

/* prints some details from the cm id */
void show_rdma_cmid(struct rdma_cm_id *id);


#endif /* RDMA_COMMON_H */