#ifndef ANPNETSTACK_TCB_STORE_H
#define ANPNETSTACK_TCB_STORE_H

#include "systems_headers.h"
#include "tcb.h"

#define MAX_TCB_CNT 100

struct tcb_store {
    _Atomic uint8_t len;
    struct tcb** tcb_list;
    pthread_mutex_t* locks;
    pthread_cond_t* signals;
};

void init_tcb_store();
int8_t new_tcb_idx();
struct tcb* get_tcb_at_idx(uint8_t idx, bool lock);
void unlock_tcb_at_idx(uint8_t idx);
struct tcb* get_tcb_by_connection(uint32_t src_addr,
                                  uint32_t dst_addr,
                                  uint16_t src_port,
                                  uint16_t dst_port);

bool is_associated_tcb(struct tcb*,
                       uint32_t src_addr,
                       uint32_t dst_addr,
                       uint16_t src_port,
                       uint16_t dst_port);

bool is_valid_tcb(uint8_t idx);
int init_tcb_idx(uint8_t idx);
void wait_for_tcb_change(uint8_t tcb_idx);

extern struct tcb_store* global_tcb_store;

#endif //ANPNETSTACK_TCB_STORE_H
