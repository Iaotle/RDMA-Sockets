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

#include "arp.h"
#include "anp_netdev.h"
#include "subuff.h"
#include "linklist.h"

#include "systems_headers.h"
#include "ethernet.h"
#include "utilities.h"

/// ARP is defined in : https://tools.ietf.org/html/rfc826

static uint8_t broadcast_hw[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static LIST_HEAD(arp_cache);

// allocate an ARP packet
static struct subuff *alloc_arp_sub()
{
    struct subuff *sub = alloc_sub(ETH_HDR_LEN + ARP_HDR_LEN + ARP_DATA_LEN);
    sub_reserve(sub, ETH_HDR_LEN + ARP_HDR_LEN + ARP_DATA_LEN);
    sub->protocol = htons(ETH_P_ARP);
    return sub;
}

static int process_arp_entry(struct arp_hdr *hdr, struct arp_ipv4 *data){
    struct list_head *item;
    struct arp_cache_entry *entry;
    list_for_each(item, &arp_cache) {
        entry = list_entry(item, struct arp_cache_entry, list);
        if (entry->arpIpv4.src_ip == data->src_ip) {
            printf("ARP an entry updated \n");
            memcpy(entry->arpIpv4.src_mac, data->src_mac, 6);
            // if it matches we consumed it
            return 0;
        }
    }
    // if we are here then we did not consume it, insert a new entry
    entry = calloc(1, sizeof(struct arp_cache_entry));
    list_init(&entry->list);
    entry->state = ARP_RESOLVED;
    memcpy(&entry->arpIpv4, data, sizeof(*data));
    list_add_tail(&entry->list, &arp_cache);
    u32_ip_to_str("[ARP] A new entry for", data->src_ip);
    debug_arp_payload("original ", data);
    debug_arp_payload("saved ", (&(entry->arpIpv4)));
    return 0;
}

void arp_init()
{

}

void arp_rx(struct subuff *skb)
{
    struct arp_hdr *arphdr;
    struct arp_ipv4 *arpdata;
    struct anp_netdev *netdev;
    arphdr = arp_hdr(skb);
    // get the host ordering -- network operations are done in the network byte order
    arphdr->hwtype = ntohs(arphdr->hwtype);
    arphdr->protype = ntohs(arphdr->protype);
    arphdr->opcode = ntohs(arphdr->opcode);
    debug_arp("in", arphdr);

    if (arphdr->hwtype != ARP_ETHERNET) {
        printf("Error: not a Ethernet type ARP, how did it end up here?\n");
        goto drop_pkt;
    }

    if (arphdr->protype != ARP_IPV4) {
        printf("Error: In ARP, not an IPv4 protocol, dropping message \n");
        goto drop_pkt;
    }

    //https://en.wikipedia.org/wiki/Address_Resolution_Protocol
    //https://bidsarmanish.blogspot.com/2016/07/arp-working-example-qa-address.html
    arpdata = (struct arp_ipv4 *) &arphdr->data;

    arpdata->src_ip = ntohl(arpdata->src_ip);
    arpdata->dst_ip = ntohl(arpdata->dst_ip);
    debug_arp_payload("receive", arpdata);
    process_arp_entry(arphdr, arpdata);
    // now check what else needs to be done
    switch (arphdr->opcode) {
        case ARP_REQUEST:
            if (!(netdev = netdev_get(arpdata->dst_ip))) {
                printf("This ARP request was not for us, dropping \n");
                goto drop_pkt;
            }
            // otherwise reply
            arp_reply(skb, netdev);
            return;
        case ARP_REPLY:
            // we already processed the reply
            goto drop_pkt;
        default:
            printf("ARP: Opcode not supported\n");
            goto drop_pkt;
    }

    drop_pkt:
    free_sub(skb);
}

int arp_request(uint32_t src_ip, uint32_t dst_ip, struct anp_netdev *netdev)
{
    struct subuff *sub;
    struct arp_hdr *arp;
    struct arp_ipv4 *payload;
    int rc = 0;

    sub = alloc_arp_sub();
    if (!sub) {
        printf("Error: allocation of the arp sub in request failed \n");
        return -1;
    }

    sub->dev = netdev;
    // by pushing by the ARP data len we are the payload starting
    payload = (struct arp_ipv4 *) sub_push(sub, ARP_DATA_LEN);

    // well we do not know the destination address, hence broadcast
    memcpy(payload->dst_mac, broadcast_hw, netdev->addr_len);
    payload->dst_ip = dst_ip;

    // copy our information
    memcpy(payload->src_mac, netdev->hwaddr, netdev->addr_len);
    payload->src_ip = src_ip;

    // push again to get to the ARP header
    arp = (struct arp_hdr *) sub_push(sub, ARP_HDR_LEN);

    debug_arp("req", arp);
    arp->opcode = htons(ARP_REQUEST);
    arp->hwtype = htons(ARP_ETHERNET);
    arp->protype = htons(ETH_P_IP);
    arp->hwsize = netdev->addr_len;
    arp->prosize = 4;

    debug_arp_payload("req", payload);
    payload->src_ip = htonl(payload->src_ip);
    payload->dst_ip = htonl(payload->dst_ip);

    rc = netdev_transmit(sub, broadcast_hw, ETH_P_ARP);
    // synchronous transmission, then free
    free_sub(sub);
    return rc;
}

void arp_reply(struct subuff *sub, struct anp_netdev *netdev)
{
    struct arp_ipv4 *arpdata = NULL;
    struct arp_hdr *arphdr = arp_hdr(sub);

    // get to the end
    sub_reserve(sub, ETH_HDR_LEN + ARP_HDR_LEN + ARP_DATA_LEN);
    // push back to the ARP packet + payload
    sub_push(sub, ARP_HDR_LEN + ARP_DATA_LEN);

    arpdata = (struct arp_ipv4 *) &arphdr->data;

    memcpy(arpdata->dst_mac, arpdata->src_mac, 6);
    // the outgoing packet's destination IP is where it came from
    // IP's are in N, because we flipped it in the previous processing
    arpdata->dst_ip = arpdata->src_ip;
    // source mac is our address
    memcpy(arpdata->src_mac, netdev->hwaddr, 6);
    arpdata->src_ip = netdev->addr;

    arphdr->opcode = ARP_REPLY;

    debug_arp("reply", arphdr);
    arphdr->opcode = htons(arphdr->opcode);
    arphdr->hwtype = htons(arphdr->hwtype);
    arphdr->protype = htons(arphdr->protype);

    debug_arp_payload("reply", arpdata);
    arpdata->src_ip = htonl(arpdata->src_ip);
    arpdata->dst_ip = htonl(arpdata->dst_ip);

    sub->dev = netdev;
    netdev_transmit(sub, arpdata->dst_mac, ETH_P_ARP);
    free_sub(sub);
}

/*
 * Returns the HW address of the given source IP address
 * NULL if not found
 */
unsigned char* arp_get_hwaddr(uint32_t lookup_ip)
{
    struct list_head *item;
    struct arp_cache_entry *entry;
    list_for_each(item, &arp_cache) {
        entry = list_entry(item, struct arp_cache_entry, list);
        if (entry->state == ARP_RESOLVED &&
            entry->arpIpv4.src_ip == lookup_ip) {
            uint8_t *copy = (uint8_t *) &entry->arpIpv4.src_mac;
            return copy;
        }
    }
    // no entry found
    return NULL;
}

void free_arp_cache()
{
    struct list_head *item, *tmp;
    struct arp_cache_entry *entry;
    list_for_each_safe(item, tmp, &arp_cache) {
        entry = list_entry(item, struct arp_cache_entry, list);
        list_del(item);
        free(entry);
    }
}