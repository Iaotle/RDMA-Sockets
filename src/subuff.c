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
#include "subuff.h"
#include "linklist.h"

void free_sub(struct subuff *sub)
{
    //printf(" >> %s : freeing the sub at %p \n", __FUNCTION__, sub);
    free(sub->head);
    free(sub);
}

void *sub_reserve(struct subuff *sub, unsigned int len)
{
    sub->data += len;
    return sub->data;
}

uint8_t *sub_head(struct subuff *sub)
{
    return sub->head;
}

uint8_t *sub_push(struct subuff *sub, unsigned int len)
{
    sub->data -= len;
    sub->len += len;
    return sub->data;
}

struct subuff *alloc_sub(unsigned int size)
{
    struct subuff *sub = calloc(sizeof(*sub), 1);
    sub->data = calloc(size, 1);
    sub->head = sub->data;
    sub->end = sub->data + size;
    list_init(&sub->list);
    return sub;
}
