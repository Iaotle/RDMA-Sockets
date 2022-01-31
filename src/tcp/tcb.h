#ifndef ANPNETSTACK_TCB_H
#define ANPNETSTACK_TCB_H

#include "systems_headers.h"
#include "ringbuffer.h"

#define TCP_RCV_BUFF_SIZE 8000

enum tcp_state {
    TCP_STATE_LISTEN, 
    TCP_STATE_SYN_SENT, 
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSED
};

struct tcb {
    enum tcp_state state;

    struct subuff_head *sub_queue;

    uint32_t local_addr;
    uint32_t remote_addr;
    uint16_t local_port;
    uint16_t remote_port;

    uint32_t iss;
    uint32_t local_una;
    uint32_t local_nxt;

    uint32_t irs;
    uint32_t remote_nxt;
    uint32_t remote_wnd;
    
    struct ringbuffer* rx_buff;

    uint8_t idx;

};

struct tcb* init_tcb(uint8_t idx);
void free_tcb(struct tcb* tcb);

int set_tcb_conn(struct tcb* tcb,
                 uint32_t local_addr,
                 uint32_t remote_addr,
                 uint16_t local_port,
                 uint16_t remote_port);
void update_tcb_state(struct tcb* tcb, enum tcp_state state);
void free_tcb_subs(struct tcb* tcb);

#endif //ANPNETSTACK_TCB_H
