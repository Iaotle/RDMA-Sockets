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

#include "systems_headers.h"
#include "linklist.h"
#include "route.h"
#include "tap_netdev.h"
#include "anp_netdev.h"
#include "utilities.h"
#include "config.h"

static LIST_HEAD(routes);

extern struct anp_netdev *cdev_lo;
extern struct anp_netdev *cdev_ext;

static struct rtentry *route_alloc(uint32_t dst, uint32_t gateway, uint32_t netmask,
                                   uint8_t flags, struct anp_netdev *dev)
{
    struct rtentry *rt = malloc(sizeof(struct rtentry));
    list_init(&rt->list);

    rt->dst = dst;
    u32_ip_to_str("GXXX ", gateway);
    rt->gateway = gateway;
    rt->netmask = netmask;
    rt->flags = flags;
    rt->dev = dev;
    return rt;
}

void route_add(uint32_t dst, uint32_t gateway, uint32_t netmask, uint8_t flags,
               struct anp_netdev *dev)
{
    struct rtentry *rt = route_alloc(dst, gateway, netmask, flags, dev);
    list_add_tail(&rt->list, &routes);
}

void route_init()
{
    // local delivery over loopback
    route_add(cdev_lo->addr, 0, 0xff000000, RT_LOOPBACK, cdev_lo);
    //local deliver over IP
    route_add(cdev_ext->addr, 0, 0xffffff00, RT_HOST, cdev_ext);
    //check route information: route -n | grep 'UG[ \t]' | awk '{print $2}'
    //outside
    route_add(0, ip_str_to_h32(ANP_IP_TAP_DEV), 0, RT_GATEWAY, cdev_ext);
}

struct rtentry *route_lookup(uint32_t daddr)
{
    struct list_head *item;
    struct rtentry *rt = NULL;
    list_for_each(item, &routes) {
        rt = list_entry(item, struct rtentry, list);
        if ((daddr & rt->netmask) == (rt->dst & rt->netmask)) break;
        // If no matches, we default to to default gw (last item)
    }
    return rt;
}

void free_routes()
{
    struct list_head *item, *tmp;
    struct rtentry *rt;

    list_for_each_safe(item, tmp, &routes) {
        rt = list_entry(item, struct rtentry, list);
        list_del(item);
        free(rt);
    }
}

