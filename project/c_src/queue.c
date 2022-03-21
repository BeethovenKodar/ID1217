
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "queue.h"


//* enqueue sorted ordering, ascending or descending
void enqueue_sorted(queue_t *queue, queue_entry_t *elem, int direction) {
    if (queue->first == NULL || queue->first->floor * direction >= elem->floor * direction) {
        elem->next = queue->first;
        queue->first = elem;
        return;
    }

    queue_entry_t *current = queue->first;
    while (current->next != NULL && current->next->floor * direction < elem->floor * direction) {
        current = current->next;
    }
    
    elem->next = current->next;
    current->next = elem;
}

//* remove first in queue
queue_entry_t *dequeue(queue_t *queue) {
    assert(queue->first != NULL);

    queue_entry_t *remove = queue->first;
    if (queue->first->next == NULL) queue->first = NULL;
    else queue->first = queue->first->next;
    return remove;
}

//* set type of queue_entry to source or destination
void set_type(queue_t *queue, int floor, int type) {
    assert(queue->first != NULL);
    queue_entry_t *current = queue->first;
    while (current != NULL) {
        if (current->floor == floor) {
            current->type_of_req = type;
            return;
        }
        current = current->next;
    }
    assert(0);
}

//* checks if a floor is already in the queue */
int contains(queue_t *queue, int floor) {
    queue_entry_t *current = queue->first;
    while (current != NULL) {
        if (current->floor == floor) return 1;
        current = current->next;
    }
    return 0;
}

//* check if queue is empty
int is_empty(queue_t *queue) {
    return (queue->first == NULL) ? 1 : 0;
}

//* show next entry floor in queue
int peek_next_stop(queue_t *queue) {
    assert(queue->first != NULL);
    return queue->first->floor;
}

//* show last entry floor in queue
int peek_last_stop(queue_t *queue) {
    assert(queue->first != NULL);

    queue_entry_t *current = queue->first;
    while (current->next != NULL) {
        current = current->next;
    }
    return current->floor;
}

//* get number of queue entries in queue
int get_num_stops(queue_t *queue) {
    queue_entry_t *current = queue->first;
    int stops = 0;
    while (current != NULL) {
        stops++;
        current = current->next;
    }
    return stops;
}