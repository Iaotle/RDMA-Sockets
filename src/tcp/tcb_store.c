#include "tcb_store.h"
#include "isn.h"

struct tcb_store* global_tcb_store;

void init_tcb_store()
{
    global_tcb_store = (struct tcb_store*) malloc(sizeof(struct tcb_store));
    global_tcb_store->len = 0;
    global_tcb_store->tcb_list = (struct tcb**) calloc(MAX_TCB_CNT, sizeof(struct tcb*));
    global_tcb_store->locks = (pthread_mutex_t*) calloc(MAX_TCB_CNT, sizeof(pthread_mutex_t));
    global_tcb_store->signals = (pthread_cond_t*) calloc(MAX_TCB_CNT, sizeof(pthread_cond_t));
    
    printf("Initialized TCB store\n");
    return;
}

int8_t new_tcb_idx()
{
    uint8_t new_idx = global_tcb_store->len++;
    if(new_idx >= MAX_TCB_CNT){
        global_tcb_store->len--;
        return -1;
    }
    
    pthread_mutex_init(&global_tcb_store->locks[new_idx], NULL);
    pthread_cond_init(&global_tcb_store->signals[new_idx], NULL);
    return new_idx;
}

int init_tcb_idx(uint8_t idx)
{
    if(get_tcb_at_idx(idx, true) != NULL){
        unlock_tcb_at_idx(idx);
        return -1;
    }
    
    global_tcb_store->tcb_list[idx] = init_tcb(idx);
    
    unlock_tcb_at_idx(idx);
    return 0;
}


bool is_valid_tcb(uint8_t idx)
{
    return (idx >= 0 && idx < global_tcb_store->len);
}

// Remember to call unlock_tcb_at_idx when tcb is not required anymore 
// after calling get_tcb_at_idx with lock=true.
struct tcb* get_tcb_at_idx(uint8_t idx, bool lock)
{
    if(!is_valid_tcb(idx)){
        return NULL;
    }

    if(lock){
        pthread_mutex_lock(&global_tcb_store->locks[idx]);
    }
    return global_tcb_store->tcb_list[idx];
}

void unlock_tcb_at_idx(uint8_t idx)
{
    pthread_mutex_unlock(&global_tcb_store->locks[idx]);
    return;
}

struct tcb* get_tcb_by_connection(uint32_t src_addr,
                                  uint32_t dst_addr,
                                  uint16_t src_port,
                                  uint16_t dst_port)
{
    for(int i = 0; i < global_tcb_store->len; i++){
        struct tcb* tcb = get_tcb_at_idx(i, true);
        if(is_associated_tcb(tcb, src_addr, dst_addr, src_port, dst_port)){
            return tcb;
        }
        unlock_tcb_at_idx(i);
    }
    return NULL;
}

bool is_associated_tcb(struct tcb* tcb,
                       uint32_t src_addr,
                       uint32_t dst_addr,
                       uint16_t src_port,
                       uint16_t dst_port)
{
    return (
        tcb->local_addr == dst_addr && 
        tcb->remote_addr == src_addr && 
        tcb->local_port == dst_port && 
        tcb->remote_port == src_port
    );
}

void wait_for_tcb_change(uint8_t tcb_idx){
    pthread_cond_wait(&global_tcb_store->signals[tcb_idx], &global_tcb_store->locks[tcb_idx]);
}
