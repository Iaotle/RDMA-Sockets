#include "tcp.h"
#include "ip/ip.h"
#include "utilities.h"
#include "timer.h"
#include "anp_netdev.h"
#include "tcb_store.h"
#include "ringbuffer.h"
#include "config.h"

void tcp_rx(struct subuff *sub)
{
    struct tcp* tcph = TCP_HDR_FROM_SUB(sub);
    struct iphdr* ip = IP_HDR_FROM_SUB(sub);

    int checksum = do_tcp_csum((uint8_t*) tcph, IP_PAYLOAD_LEN(ip), ip->proto, htonl(ip->saddr), htonl(ip->daddr));
    if (checksum != 0){
        goto drop_packet;
    }

    tcph->src_port = ntohs(tcph->src_port);
    tcph->dst_port = ntohs(tcph->dst_port);
    tcph->seq_nr = ntohl(tcph->seq_nr);
    tcph->ack_nr = ntohl(tcph->ack_nr);
    tcph->window_size = ntohs(tcph->window_size);
    tcph->checksum = ntohs(tcph->checksum);
    tcph->urgent_ptr = ntohs(tcph->urgent_ptr);

    struct tcb* tcb = get_tcb_by_connection(ip->saddr, ip->daddr, tcph->src_port, tcph->dst_port);
    if (tcb == NULL){
        goto drop_packet;
    }
    tcp_process_rx(sub, tcb);
    unlock_tcb_at_idx(tcb->idx);
    
    drop_packet:
    free_sub(sub);
}

void tcp_process_rx(struct subuff* sub, struct tcb* tcb)
{
    struct tcp* tcph = TCP_HDR_FROM_SUB(sub);
    struct iphdr* ip = IP_HDR_FROM_SUB(sub);

    tcb->remote_wnd = tcph->window_size;

    if (TCP_HAS_FLAG(tcph, TCP_SYN)) {
        handle_incoming_syn(tcb, tcph);
    }

    if (TCP_HAS_FLAG(tcph, TCP_ACK)) {
        handle_incoming_ack(tcb, tcph);
    }

    if (TCP_HAS_FLAG(tcph, TCP_FIN)) {
        handle_incoming_fin(tcb, tcph);
    }

    if(TCP_PAYLOAD_SIZE(ip, tcph) > 0){
        handle_incoming_data(tcb, tcph, ip);
    }

}

void handle_incoming_syn(struct tcb* tcb, struct tcp* tcph)
{
    if(tcb->state == TCP_STATE_SYN_SENT || tcb->state == TCP_STATE_SYN_RECEIVED){
        tcb->irs = tcph->seq_nr;
        tcb->remote_nxt = tcb->irs + 1;

        tcp_send_ack(tcb);
        update_tcb_state(tcb, TCP_STATE_SYN_RECEIVED);   
    }
    
}

void handle_incoming_ack(struct tcb* tcb, struct tcp* tcph)
{
    if (tcph->ack_nr == tcb->local_nxt){
        if(tcb->state == TCP_STATE_FIN_WAIT_1){
            update_tcb_state(tcb, TCP_STATE_FIN_WAIT_2);
        }
        else if(tcb->state == TCP_STATE_CLOSING) {
            update_tcb_state(tcb, TCP_STATE_TIME_WAIT);
            start_time_wait_timer(tcb->idx);
        }
    }

    if(SEQ_LEQ(tcb->local_una, tcph->ack_nr) && SEQ_LEQ(tcph->ack_nr, tcb->local_nxt)) {
        tcb->local_una = tcph->ack_nr + 1;
        remove_acked_subs(tcb);
        if(tcb->state == TCP_STATE_SYN_RECEIVED) {
            update_tcb_state(tcb, TCP_STATE_ESTABLISHED);
        }
    }
}

void handle_incoming_fin(struct tcb* tcb, struct tcp* tcph)
{
    if(tcph->seq_nr != tcb->remote_nxt) {
        return;
    }
    tcb->remote_nxt += 1;
    if (tcb->state == TCP_STATE_FIN_WAIT_1) {
        update_tcb_state(tcb,TCP_STATE_CLOSING);
    } else if (tcb->state == TCP_STATE_FIN_WAIT_2) {
        update_tcb_state(tcb, TCP_STATE_TIME_WAIT);
        start_time_wait_timer(tcb->idx);
    }
    tcp_send_ack(tcb);
}

void handle_incoming_data(struct tcb* tcb, struct tcp* tcph, struct iphdr* ip)
{
    if(tcb->remote_nxt == tcph->seq_nr){       
        uint32_t ret_bytes = ringbuffer_add(tcb->rx_buff, TCP_PAYLOAD_FROM_HDR(tcph), TCP_PAYLOAD_SIZE(ip,tcph));
        tcb->remote_nxt = tcb->remote_nxt + ret_bytes;
        update_tcb_state(tcb, tcb->state);
    }
    tcp_send_ack(tcb);
}

void remove_acked_subs(struct tcb* tcb)
{
    pthread_mutex_lock(&tcb->sub_queue->lock);
    struct list_head *item, *tmp;
    struct subuff* entry;
    list_for_each_safe(item, tmp, &tcb->sub_queue->head){
        entry = list_entry(item, struct subuff, list);
        if(SEQ_LEQ(entry->end_seq, tcb->local_una)){
            timer_cancel(entry->timer);
            list_del(item);
            free(entry);
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&tcb->sub_queue->lock);
}

void start_time_wait_timer(uint8_t tcb_idx) {
    timer_add(2 * TCP_MSL_MSECS, close_tcp_connection, (void*)(uint64_t)tcb_idx);
}

void* close_tcp_connection(void* arg){
    uint64_t tcb_idx = (uint64_t)arg;
    struct tcb* tcb = get_tcb_at_idx(tcb_idx, true);
    if(tcb){
        global_tcb_store->tcb_list[tcb_idx] = NULL;
        free_tcb(tcb);
        pthread_cond_signal(&global_tcb_store->signals[tcb->idx]);
    }
    
    unlock_tcb_at_idx(tcb_idx);
    return NULL;
}