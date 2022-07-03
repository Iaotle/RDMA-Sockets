/**
 * Linked list implementation adapted from https://www.tutorialspoint.com/data_structures_algorithms/linked_list_program_in_c.htm
 */

#include "socklist.h"

node* head = NULL;
node* current = NULL;

// insert link at the first location with socket
sock* create(sock* socket) {
    // create a link
    node* link = (node*)malloc(sizeof(node));
    if (!link) {
        printf("Failed to create node\n");
        return NULL;
    }
    link->socket = socket;
    // link->data = data;

    // point it to old first node
    link->next = head;

    // point first to new first node
    head = link;

    return link->socket;
}

// is list empty
bool isEmpty() { return head == NULL; }

int length() {
    int length = 0;
    node* current;

    for (current = head; current != NULL; current = current->next) {
        length++;
    }

    return length;
}

// find a sock with a given fd
sock* find(int fd) {
    // start from the first link
    node* current = head;

    // if list is empty
    if (head == NULL) {
        return NULL;
    }

    // navigate through list
    while (current->socket->fd != fd) {
        // if it is last node
        if (current->next == NULL) {
            return NULL;
        } else {
            // go to next link
            current = current->next;
        }
    }

    // if data found, return the current Link
    return current->socket;
}

// delete a link with given key. MAKE SURE TO FREE SOCKET BEFORE CALLING THIS

void delete (int key) {
    // start from the first link
    node* current = head;
    node* previous = NULL;

    // if list is empty
    if (head == NULL) {
        return;
    }

    // navigate through list
    while (current->socket->fd != key) {
        // if it is last node
        if (current->next == NULL) {
            return;
        } else {
            // store reference to current link
            previous = current;
            // move to next link
            current = current->next;
        }
    }

    // found a match, update the link
    if (current == head) {
        // change first to point to next link
        head = head->next;
    } else {
        // bypass the current link
        previous->next = current->next;
    }
    free(current->socket);
    free(current);
    return;
}