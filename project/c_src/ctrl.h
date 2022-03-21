
#ifndef __CTRL_H
    #define __CTRL_H

#include <pthread.h>

/* program boundaries */
int ELEVATORS;
int LEVELS;
char *HOSTNAME;
int PORT;

typedef enum {SRC, DEST} type_of_req;
typedef enum {UP_NOW, UP_LATER, DOWN_NOW, DOWN_LATER} queue_type;

typedef struct queue_entry_t {
    int floor;
    type_of_req type_of_req;
    struct queue_entry_t *next;
} queue_entry_t;

typedef struct queue_t {
    queue_type type;
    queue_entry_t *first;
} queue_t;

typedef struct elevator_state_t {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int eid;
    int emergency_stop;
    double current_position;
    int direction;
    queue_t *up_now;
    queue_t *down_now;
    queue_t *up_later;
    queue_t *down_later;
    queue_t *current_queue;
} elevator_state_t;

typedef struct cost_result_t {
    double cost;
    queue_t *queue;
} cost_result_t;

#endif