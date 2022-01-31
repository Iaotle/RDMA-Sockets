#ifndef ANPNETSTACK_ANP_RINGBUFFER_H
#define ANPNETSTACK_ANP_RINGBUFFER_H

//#include "systems_headers.h"
#include <stdint.h>
#include <stddef.h>

struct ringbuffer {
    uint8_t* data; // Allocated array is 1 byte bigger to accomodate for full and empty states
    uint32_t len; // Length is the usable data length + 1 (actual length of allocated data array)
    uint32_t begin;
    uint32_t end;
};

struct ringbuffer* ringbuffer_alloc(size_t size);
void ringbuffer_free(struct ringbuffer* rbuf);

/**
* Add data to the ringbuffer.
* @param rbuf the ring buffer pointer
* @param data the data to copy into the buffer
* @param dsize the amoount of bytes to add
* @return the amount of bytes added to the buffer
*/
int ringbuffer_add(struct ringbuffer* rbuf, uint8_t* data, size_t dsize);

/**
* Remove data from the ringbuffer.
* @param rbuf the ring buffer pointer
* @param outbuf an output buffer where the removed data can be copied into
* @param size the amount of bytes to remove
* @return the amount of bytes removed from the ring buffer (and copied to the output buffer)
*/
int ringbuffer_remove(struct ringbuffer* rbuf, uint8_t* outbuf, size_t size);

int ringbuffer_empty(struct ringbuffer* rbuf);
int ringbuffer_full(struct ringbuffer* rbuf);

/**
 * @return how many unused bytes are in the ring buffer
 */
int ringbuffer_unused_elem_count(struct ringbuffer* rbuf);

#endif // ANPNETSTACK_ANP_RINGBUFFER_H
