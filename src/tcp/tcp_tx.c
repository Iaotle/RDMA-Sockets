#include "tcp.h"
#include "ip/ip.h"
#include "utilities.h"
#include "timer.h"
#include "anp_netdev.h"
#include "arp/arp.h"
#include "tcb_store.h"

void tcp_output(struct subuff* sub, struct tcb* tcb, uint8_t flags)
{
    sub_push(sub, TCP_HDR_SIZE_MIN);
    sub->protocol = IPP_TCP;

    struct tcp *tcph = TCP_HDR_FROM_SUB(sub);
    tcph->flags = flags;
    tcph->src_port = tcb->local_port;
    tcph->dst_port = tcb->remote_port;
    tcph->seq_nr = tcb->local_nxt;
    tcph->ack_nr = tcb->remote_nxt;
    tcph->data_off = TCP_DATA_OFF_MIN;
    tcph->window_size = ringbuffer_unused_elem_count(tcb->rx_buff);
    tcph->checksum = 0;

    sub->seq = tcph->seq_nr;
    sub->end_seq = sub->seq + sub->dlen;
	
    tcb->local_nxt += sub->dlen;
    if(TCP_HAS_FLAG(tcph, TCP_SYN) || TCP_HAS_FLAG(tcph, TCP_FIN)){
        tcb->local_nxt++;
        sub->end_seq++;
    }

    tcph->src_port = htons(tcph->src_port);
    tcph->dst_port = htons(tcph->dst_port);
    tcph->seq_nr = htonl(tcph->seq_nr);
    tcph->ack_nr = htonl(tcph->ack_nr);
    tcph->window_size = htons(tcph->window_size);
    tcph->urgent_ptr = htons(tcph->urgent_ptr);
    tcph->checksum = do_tcp_csum((uint8_t*) tcph, TCP_HDR_SIZE_MIN + sub->dlen, IPP_TCP, htonl(tcb->local_addr), htonl(tcb->remote_addr));

    ip_output(tcb->remote_addr, sub);
    unlock_tcb_at_idx(sub->tcb_idx);
}

void send_tcp_packet(struct subuff* sub, struct tcb* tcb, uint8_t flags)
{
    tcp_output(sub, tcb, flags);
    sub->timer = timer_add(TCP_RESUBMIT_INTERVAL, resub_tcp_packet, sub);
    pthread_mutex_lock(&tcb->sub_queue->lock);
    sub_queue_tail(tcb->sub_queue, sub);
    pthread_mutex_unlock(&tcb->sub_queue->lock);
}

void* resub_tcp_packet(void* args)
{
    struct subuff* sub = (struct subuff*) args;
    uint8_t tcb_idx = sub->tcb_idx;
    struct tcb* tcb = get_tcb_at_idx(tcb_idx, true);
    if(!tcb){
        printf("Error: TCB not found at idx: %d\n", tcb_idx);
        goto unlock_and_stop;
    }
    
    struct tcp* tcph = TCP_HDR_FROM_SUB(sub);
    if(SEQ_LT(sub->end_seq, tcb->local_una)){
        printf("Packet is ACK'd, resub stops (end_seq=%u, local_una=%u)\n", sub->end_seq, tcb->local_una);
        sub->timer = NULL; 
        goto unlock_and_stop;
    }
    
    // Also check if receiver window is full. If so, then reset ttl and requeue resubmit.
    if(tcb->remote_wnd == 0){
        printf("Remote window size is 0, packet on hold...\n");
        sub->ttl = TCP_RETRANSMIT_TTL; 
        sub->timer = timer_add(TCP_RESUBMIT_INTERVAL, resub_tcp_packet, sub);
        goto unlock_and_stop;
    }

    if(sub->ttl > 0){
        printf("Resubmitting packet...\n");
        sub->ttl--;
        // Rereserve sub and push
		sub->data += sub->len - sub->dlen - (tcph->data_off >> 4) * 4;
		sub->len = sub->dlen + (tcph->data_off >> 4) * 4;
        
        ip_output(tcb->remote_addr, sub);

        sub->timer = timer_add(TCP_RESUBMIT_INTERVAL, resub_tcp_packet, sub);
    } else {
        printf("Max times of retransmit reached, closing connection\n");
        sub->timer = NULL;
        // Close tcb in different thread to prevent deadlock
        timer_add(0, close_tcp_connection, (void*)(uint64_t)tcb_idx);
    }

    unlock_and_stop:
    unlock_tcb_at_idx(tcb_idx);
    return NULL;
}

struct subuff *alloc_tcp_sub(size_t size, uint8_t tcb_idx, uint32_t dlen, uint8_t ttl)
{
    struct subuff *sub = alloc_sub(size);
    sub->dlen = dlen;
    sub->tcb_idx = tcb_idx;
    sub->ttl = ttl;
    return sub;
}

void tcp_send_ack(struct tcb* tcb)
{
    struct subuff* sub = alloc_tcp_sub(TCP_PACKET_SIZE(0), tcb->idx, 0, TCP_RETRANSMIT_TTL);
    sub_reserve(sub, TCP_PACKET_SIZE(0));
    tcp_output(sub, tcb, TCP_ACK);
}

void tcp_send_fin(struct tcb* tcb){
    update_tcb_state(tcb, TCP_STATE_FIN_WAIT_1);
    struct subuff* sub = alloc_tcp_sub(TCP_PACKET_SIZE(0), tcb->idx, 0, TCP_RETRANSMIT_TTL);
    sub_reserve(sub, TCP_PACKET_SIZE(0));
    send_tcp_packet(sub, tcb, TCP_FIN | TCP_ACK);
}

void tcp_send_syn(struct tcb* tcb){
    struct subuff* sub = alloc_tcp_sub(TCP_PACKET_SIZE(0), tcb->idx, 0, TCP_RETRANSMIT_TTL);
	sub_reserve(sub, TCP_PACKET_SIZE(0));
    send_tcp_packet(sub, tcb, TCP_SYN);
}
