#include "ringbuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../utilities.h"

struct ringbuffer* ringbuffer_alloc(size_t size) {
    struct ringbuffer* rbuf = (struct ringbuffer*) malloc(sizeof(struct ringbuffer));
    if(!rbuf) {
        return NULL;
    }

    rbuf->data = (uint8_t*) malloc(size + 1);
    if(!rbuf->data) {
        free(rbuf);
        return NULL;
    }

    rbuf->len = size + 1;
    rbuf->begin = 0;
    rbuf->end = 0;
    return rbuf;
}

void ringbuffer_free(struct ringbuffer* rbuf) {
	free(rbuf->data);
	free(rbuf);
}

int ringbuffer_add(struct ringbuffer* rbuf, uint8_t* data, size_t dsize) {
	int max_off = ringbuffer_unused_elem_count(rbuf);
    int off = MIN(dsize, max_off);
	int modulo_off = 0;
	if(rbuf->end + off > rbuf->len) {
		modulo_off = rbuf->end + off - rbuf->len;
		off = rbuf->len - rbuf->end;
	}

	memcpy(rbuf->data + rbuf->end, data, off);
	memcpy(rbuf->data, data + off, modulo_off);

	rbuf->end = (rbuf->end + off + modulo_off) % rbuf->len;
	return off + modulo_off;
}

int ringbuffer_remove(struct ringbuffer* rbuf, uint8_t* outbuf, size_t size) {
	int max_off = (rbuf->len - ringbuffer_unused_elem_count(rbuf)) - 1;
	int off = MIN(size, max_off);
	int modulo_off = 0;
	if(rbuf->begin + off > rbuf->len) {
		modulo_off = rbuf->begin + off - rbuf->len;
		off = rbuf->len - rbuf->begin;
	}

	memcpy(outbuf, rbuf->data + rbuf->begin, off);
	memcpy(outbuf + off, rbuf->data, modulo_off);

	rbuf->begin = (rbuf->begin + off + modulo_off) % rbuf->len;
	return off + modulo_off;
}

int ringbuffer_unused_elem_count(struct ringbuffer* rbuf) {
	if(rbuf->end < rbuf->begin) {
		return rbuf->begin - rbuf->end - 1;
	}
	return rbuf->len - (rbuf->end - rbuf->begin) - 1;
}

int ringbuffer_empty(struct ringbuffer* rbuf) {
	return ringbuffer_unused_elem_count(rbuf) == (rbuf->len - 1);
}

int ringbuffer_full(struct ringbuffer* rbuf) {
	return ringbuffer_unused_elem_count(rbuf) == 0;
}