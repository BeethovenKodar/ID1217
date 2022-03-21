
#ifndef __QUEUE_H
    #define __QUEUE_H

#include "ctrl.h"

void enqueue_sorted(queue_t *queue, queue_entry_t *elem, int direction);
queue_entry_t* dequeue(queue_t *queue);
void set_type(queue_t *queue, int floor, int type);
int is_empty(queue_t *queue);
int contains(queue_t *queue, int floor);
int peek_next_stop(queue_t *queue);
int peek_last_stop(queue_t *queue);
int get_num_stops(queue_t *queue);

#endif