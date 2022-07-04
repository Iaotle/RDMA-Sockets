#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Linked list implementation adapted from https://www.tutorialspoint.com/data_structures_algorithms/linked_list_program_in_c.htm
 */

#include "rdma_common.h"

typedef struct node {
    sock* socket;
    struct node* next;
} node;

sock* create(sock* socket);

bool isEmpty();

sock* find(int fd);

void delete (int key);