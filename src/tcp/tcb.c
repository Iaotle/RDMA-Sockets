#include "tcp.h"
#include "utilities.h"
#include "tcb.h"
#include "signal.h"
#include "isn.h"
#include "subuff.h"
#include "config.h"
#include "tcp/tcb_store.h"

struct tcb* init_tcb(uint8_t idx){
    struct tcb* tcb = malloc(sizeof(struct tcb));
    tcb->sub_queue = malloc(sizeof(struct subuff_head));
    tcb->rx_buff = ringbuffer_alloc(TCP_RCV_BUFF_SIZE);
    
    pthread_mutex_init(&tcb->sub_queue->lock, NULL);
    sub_queue_init(tcb->sub_queue);
    
    tcb->state = TCP_STATE_CLOSED;
    tcb->iss = isn_get();
    tcb->local_una = tcb->iss;
    tcb->local_nxt = tcb->iss;
    tcb->idx = idx;
    
    return tcb;
}

void free_tcb(struct tcb* tcb){
    ringbuffer_free(tcb->rx_buff);
    free_tcb_subs(tcb);
    free(tcb);
}

int set_tcb_conn(struct tcb* tcb,
                 uint32_t local_addr,
                 uint32_t remote_addr,
                 uint16_t local_port,
                 uint16_t remote_port)
{    
    if(!tcb){
        return -1;
    }

    tcb->local_addr = local_addr;
    tcb->remote_addr = remote_addr;
    tcb->local_port = local_port;
    tcb->remote_port = remote_port;
    tcb->remote_wnd = TCP_INIT_SEND_WND;
    
    // Ensures no other threads can connect (because state != closed) and SYN from 
    // other socket can be received already.
    tcb->state = TCP_STATE_SYN_SENT;
    
    return 0;
}

void update_tcb_state(struct tcb* tcb, enum tcp_state state) {
    tcb->state = state;
    pthread_cond_signal(&global_tcb_store->signals[tcb->idx]);
}

void free_tcb_subs(struct tcb* tcb)
{
    pthread_mutex_lock(&tcb->sub_queue->lock);
    struct list_head *item, *tmp;
    struct subuff* entry;
    list_for_each_safe(item, tmp, &tcb->sub_queue->head){
        entry = list_entry(item, struct subuff, list);
        list_del(item);
        free(entry);
    }
    pthread_mutex_unlock(&tcb->sub_queue->lock);
}
