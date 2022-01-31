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

#ifndef ANPNETSTACK_TIMER_H
#define ANPNETSTACK_TIMER_H

/*
There is a timer thread that runs in an infinite loop (timer_start) function.
The function sleeps for 10 milliseconds and maintains a tick counter.
This timer thread is created at the start of the code execution. Now, you can
schedule a callback from this facility by calling timer_add function with an
appropriate handler function. This is a function which has a signature of
void *handler(void*) and you can pass appropriate arguments (if any). You can
pass when to invoke this function after how many "expire" milliseconds
(as the internal time is maintain with the granularity of 10 millisecond).
When the timer expires, in the timer_tick function your callback handler
function will be called on a new thread, and then you can do whatever you
would have done when the timer expired, for example re-transmission of any
packet, or just ignore the timeout callback because you have already received
an ACK packet for a sent data packet. There are multiple ways you can use
this timer facility. This is a common way in which timer infrastructure is
used by passing a callback functions with timeout values. What to do with
the returned struct timer? It is up to you. Perhaps you want to keep track
of you have scheduled a timer or not? So save it in a pointer. If the pointer
is null then you have not scheduled a timer, otherwise you have. It might come
in handy if you want to cancel a timer (because you have received an ACK).
There is no right answer here. The infrastructure is provided for a maximum
freedom, feel free to ignore/modify/re-write your own as you see fit your needed.
*/

#include "systems_headers.h"
#include "linklist.h"

#define timer_dbg(msg, t)                                               \
    do {                                                                \
        print_debug("Timer at %d: "msg": expires %d", tick, t->expires); \
    } while (0)

struct timer {
    struct list_head list;
    int refcnt;
    uint32_t expires;
    int cancelled;
    void *(*handler)(void *);
    void *arg;
    pthread_mutex_t lock;
};

struct timer *timer_add(uint32_t expire, void *(*handler)(void *), void *arg);
void timer_oneshot(uint32_t expire, void *(*handler)(void *), void *arg);
void timer_release(struct timer *t);
void timer_cancel(struct timer *t);
void *timers_start();
int timer_get_tick();

#endif //ANPNETSTACK_TIMER_H
