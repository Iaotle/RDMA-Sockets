#ifndef ANPNETSTACK_TCP_H
#define ANPNETSTACK_TCP_H

#include "systems_headers.h"
#include "subuff.h"
#include "tcb.h"
#include "ip/ip.h"

#define TCP_HDR_FROM_SUB(_sub) (struct tcp*)(_sub->head + ETH_HDR_LEN + IP_HDR_LEN)
#define TCP_PAYLOAD_FROM_HDR(_tcph) (((uint8_t*) _tcph) + 4*(_tcph->data_off >> 4))
#define TCP_HAS_FLAG(_tcph, _flag) ((_tcph->flags & _flag) != 0)
#define TCP_HDR_SIZE_MIN 20
#define TCP_PACKET_SIZE(data_len) ETH_HDR_LEN + IP_HDR_LEN + TCP_HDR_SIZE_MIN + data_len
#define TCP_RESUBMIT_INTERVAL 1000
#define TCP_OVER_ETH_MAX_PAYLOAD_SIZE 1460
#define TCP_DATA_OFF_MIN 5 << 4
#define TCP_PAYLOAD_SIZE(_ip, _tcph) IP_PAYLOAD_LEN(_ip) - 4*(_tcph->data_off >> 4)
#define TCP_RETRANSMIT_TTL 15


/*
* Sequence number modulo arithmetic comparisons taken from FreeBSD kernel source code
* https://github.com/freebsd/freebsd-src/blob/main/sys/netinet/tcp_seq.h
*/
#define	SEQ_LT(a,b)	    ((int)((a)-(b)) < 0)
#define	SEQ_LEQ(a,b)    ((int)((a)-(b)) <= 0)
#define	SEQ_GT(a,b)	    ((int)((a)-(b)) > 0)
#define	SEQ_GEQ(a,b)	((int)((a)-(b)) >= 0)

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20
#define TCP_ECE 0x40
#define TCP_CWR 0x80

#define DEBUG_TCP
#ifdef DEBUG_TCP
#define debug_tcp_hdr(msg, hdr) \
    do { \
        printf("TCP "msg"- src_port: %hu, dst_port: %hu, seq_nr: %u, ack_nr: %u, " \
        "flags: %hhu, window_size: %hu, checksum: %hx, SYN: %d, ACK: %d \n" \
        ,hdr->src_port, hdr->dst_port, hdr->seq_nr, hdr->ack_nr, \
        hdr->flags, hdr->window_size, hdr->checksum, TCP_HAS_FLAG(hdr, TCP_SYN), TCP_HAS_FLAG(hdr, TCP_ACK)); \
    } while (0)

#else
#define debug_tcp_hdr(msg, hdr)
#endif

struct tcp {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_nr;
    uint32_t ack_nr;
    uint8_t data_off;
    uint8_t flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
    uint8_t data[];
} __attribute__((packed));

void tcp_rx(struct subuff *sub);
void tcp_process_rx(struct subuff* sub, struct tcb* tcb);
void tcp_output(struct subuff* sub, struct tcb* tcb, uint8_t flags);
void send_tcp_packet(struct subuff* sub, struct tcb* tcb, uint8_t flags);

void* resub_tcp_packet(void* args);
struct subuff *alloc_tcp_sub(size_t size, uint8_t tcb_idx, uint32_t dlen, uint8_t ttl);
void remove_acked_subs(struct tcb* tcb);
void tcp_send_ack(struct tcb* tcb);
void tcp_send_fin(struct tcb* tcb);
void tcp_send_syn(struct tcb* tcb);

void handle_incoming_syn(struct tcb* tcb, struct tcp* tcph);
void handle_incoming_ack(struct tcb* tcb, struct tcp* tcph);
void handle_incoming_data(struct tcb* tcb, struct tcp* tcph, struct iphdr* ip);
void handle_incoming_fin(struct tcb* tcb, struct tcp* tcph);

void start_time_wait_timer(uint8_t tcb_idx);
void* close_tcp_connection(void* arg);

#endif //ANPNETSTACK_TCP_H
