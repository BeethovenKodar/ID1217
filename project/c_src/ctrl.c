
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "hardwareAPI.h"
#include "ctrl.h"
#include "queue.h"

void test();

#define UP 1
#define DOWN -1
#define IDLE 0

#define NOW 1
#define LATER -1

int ELEVATORS;
const int DOOR_ALTER_TIME = 1;
const int ENTER_TIME = 1;
pthread_mutex_t write_out = PTHREAD_MUTEX_INITIALIZER;

//! ***************************** HELPERS **********************************************************
//! ***************************** HELPERS **********************************************************
//* convert enum of queue type to string
char *text(int type) {
    switch(type) {
        case UP_NOW:        return "UP_NOW";
        case DOWN_NOW:      return "DOWN_NOW";
        case UP_LATER:      return "UP_LATER";
        case DOWN_LATER:    return "DOWN_LATER";
        default :           return "N/A";
    }
}

//* prints current stops in given queue
void printqueue(queue_t *queue) {
    printf("[ ");
    queue_entry_t *current = queue->first;
    while(current != NULL) {
        printf("%d ", current->floor);
        current = current->next;
    }
    printf("]\n");
}

//* retrieve queue reference based on direction and when in time
queue_t *get_queue(elevator_state_t *es, int dir, int when) {
    if (dir == 1) {
        if (when == NOW) return es->up_now;
        else if (when == LATER) return es->up_later;
    } else {
        if (when == NOW) return es->down_now;
        else if (when == LATER) return es->down_later;
    }
    assert(0);
}

//! ***************************** COST **********************************************************
//! ***************************** COST **********************************************************
//* cost function to calculate total movement for an elevator to address a floor button press
void total_travel_distance(elevator_state_t *es, int req_dir, int floor, cost_result_t *result) {

    if (es->emergency_stop) { //* elevator unavailable
        result->queue = NULL;
        result->cost = 32000;
        return;
    }

    double total_dist = 0;
    int from = -1, to = -1;
    queue_t *stops;

    if (es->direction == IDLE) { //* idle elevator
        total_dist += fabs(floor - es->current_position);
        result->queue = get_queue(es, req_dir, NOW);
        result->cost = total_dist;
        printf("EID: %d | CASE: idle | ", es->eid);
        return;
    }
    
    if (req_dir == es->direction) { //! correct direction of elevator
        if (es->current_position * req_dir > floor * req_dir) { //! correct direction, already passed by
            from = es->current_position;
            stops = get_queue(es, es->direction, NOW);                  //* "correct" direction
            if (!is_empty(stops)) {
                to = peek_last_stop(stops);                             //* last stop in this direction
                total_dist += fabs(es->current_position - to);
                // printf("dist: %f (es->current=%f, to=%d)\n", total_dist, es->current_position, to);
            }

            stops = get_queue(es, es->direction * (-1), NOW);           //* opposite direction of request
            if (!is_empty(stops)) {
                if (to == -1) to = peek_next_stop(stops);               //* "correct" direction to pick up in opposite 
                else {                                                  //* had a stop in initial direction
                    from = to;
                    to = peek_last_stop(stops);
                }
                total_dist += abs(from - to);
                // printf("dist: %f (from=%d, to=%d)\n", total_dist, from, to);
            }

            stops = get_queue(es, es->direction, NOW);                  //* correct direction of request (pick up here)
            if (!is_empty(stops)) {
                int next_stop_right_dir = peek_next_stop(stops);
                if (to * req_dir > next_stop_right_dir * req_dir) {     //! is requested floor before last stop
                    to = next_stop_right_dir;
                }
            }

            from = to;
            total_dist += abs(from - floor);
            // printf("dist: %f (from=%d, floor=%d)\n", total_dist, from, floor);

            result->cost = total_dist;
            result->queue = get_queue(es, es->direction, LATER);
            printf("EID: %d | CASE: up-down-up or down-up-down! | ", es->eid);
            return; //* CASE: UP-DOWN-UP or DOWN-UP-DOWN until reqest from floor */
        } else { //! correct direction, on the way
            total_dist += fabs(floor - es->current_position);
            // printf("dist: %f (es->current=%f, floor=%d)\n", total_dist, es->current_position, floor);

            result->cost = total_dist;
            result->queue = get_queue(es, es->direction, NOW);
            printf("EID: %d | CASE: on the way | ", es->eid);
            return; //* CASE: will pass by request from floor
        }
    } else { //! incorrect direction of elevator, must turn first
        stops = get_queue(es, es->direction, NOW);                      //* "correct" direction
        if (!is_empty(stops)) {                                         //! ????
            to = peek_last_stop(stops);
        } else {
            stops = get_queue(es, es->direction * (-1), NOW);           //* requested direction
            if (!is_empty(stops)) {
                to = peek_last_stop(stops);
                int next_stop_right_dir = peek_next_stop(stops);
                if (to * req_dir > next_stop_right_dir * req_dir) {     //! is requested floor before last stop
                    to = next_stop_right_dir;
                }
            } else {
                to = floor;
            }
        }
        
        total_dist += fabs(es->current_position - to);
        // printf("dist: %f (es->current=%f, to=%d)\n", total_dist, es->current_position, to);
        from = to;
        total_dist += abs(from - floor);
        // printf("dist: %f (from=%d, floor=%d)\n", total_dist, from, floor);

        result->cost = total_dist;
        result->queue = get_queue(es, es->direction * (-1), NOW);
        printf("EID: %d | CASE: initial wrong way | ", es->eid);
        return; //* CASE: initial wrong direction, must turn first
    }
}

//* calculates total number of stops already assigned to an elevator
void total_num_stops(elevator_state_t *es, cost_result_t *result) {
    result->cost += get_num_stops(es->up_now);
    result->cost += get_num_stops(es->up_later);
    result->cost += get_num_stops(es->down_now);
    result->cost += get_num_stops(es->down_later);
}

//* finds elevator with lowest cost to requested floor
void find_elevator(int req_floor, int req_dir, elevator_state_t *elevator_states) {
    cost_result_t *candidates = (cost_result_t*)malloc(ELEVATORS * sizeof(cost_result_t));
    for (int eid = 0; eid < ELEVATORS; eid++)
        pthread_mutex_lock(&elevator_states[eid].mutex);

    //* calculate cost (travel distance in floors and number of stops currently)
    int best_eid;
    int min_cost = 1000;
    for (int eid = 0; eid < ELEVATORS; eid++) {
        total_travel_distance(&elevator_states[eid], req_dir, req_floor, &candidates[eid]);
        total_num_stops(&elevator_states[eid], &candidates[eid]);
        printf("COST: %.2f\n", candidates[eid].cost);
        if (candidates[eid].cost < min_cost) {
            best_eid = eid;
            min_cost = candidates[eid].cost;
        }
    }

    //* add if not already in queue
    if (!contains(candidates[best_eid].queue, req_floor)) {
        queue_entry_t *new_stop = (queue_entry_t*)malloc(sizeof(queue_entry_t));
        new_stop->floor = req_floor;
        new_stop->type_of_req = SRC;
        new_stop->next = NULL;
        enqueue_sorted(candidates[best_eid].queue, new_stop, req_dir);
        if (elevator_states[best_eid].direction == 0)
            pthread_cond_signal(&elevator_states[best_eid].cond);
    } else {
        set_type(candidates[best_eid].queue, req_floor, SRC);
    }

    printf("\nEID: %d assigned, COST: %.2f\n", best_eid+1, candidates[best_eid].cost);
    printf("EID: %d Added to %s ", best_eid+1, text(candidates[best_eid].queue->type)); 
    printqueue(candidates[best_eid].queue);
    printf("-------------------------------------\n\n");

    for (int eid = 0; eid < ELEVATORS; eid++)
        pthread_mutex_unlock(&elevator_states[eid].mutex);

    free(candidates);
}

//! ***************************** THREADS **********************************************************
//! ***************************** THREADS **********************************************************
//* run loop for all elevators
void *control_elevator(void *meta) {
    MotorAction ma, last_ma = 0;
    int next_stop;

    elevator_state_t *this = (elevator_state_t*)meta;
    int eid = this->eid;

    while (1) { //* until emergency stop
        pthread_mutex_lock(&this->mutex);
        if (this->direction == 0) {
            //* wait for a queue to have a job
            while (1) { 
                if (!is_empty(this->up_now)) {
                    this->current_queue = this->up_now;
                    this->direction = UP;
                    break;
                } else if (!is_empty(this->down_now)) {
                    this->current_queue = this->down_now;
                    this->direction = DOWN;
                    break;
                }
                printf("EID: %d waiting for job\n\n", eid);
                pthread_cond_wait(&this->cond, &this->mutex);
            }
        }
        
        //* lock in hand ^
        printf("EID: %d Servicing %s ", eid, text(this->current_queue->type));
        printqueue(this->current_queue);
        while (!this->emergency_stop && !is_empty(this->current_queue)) {
            next_stop = peek_next_stop(this->current_queue); //! poll if new stop has been added closer to position
            pthread_mutex_unlock(&this->mutex);
            
            ma = this->current_position < next_stop ? MotorUp : MotorDown;
            if (last_ma != ma) { //* call motor if its movement should alter
                last_ma = ma;
                pthread_mutex_lock(&write_out);
                handleMotor(eid, ma);
                pthread_mutex_unlock(&write_out);
            }

            //* check if elevator has reached next_stop, current + 0.4 > floor > current - 0.4) 
            if (this->current_position + 0.04 > next_stop && next_stop > this->current_position - 0.04) {
                pthread_mutex_lock(&write_out);
                handleMotor(eid, MotorStop);
                handleDoor(eid, DoorOpen);
                pthread_mutex_unlock(&write_out);
                last_ma = MotorStop;

                pthread_mutex_lock(&this->mutex);
                queue_entry_t *stopped_at = dequeue(this->current_queue);
                //* possibly wait for floor selection
                if (stopped_at->type_of_req == SRC) { 
                    printf("EID: %d Wait for floor selection (%d)\n", eid, this->direction);
                    pthread_cond_wait(&this->cond, &this->mutex);                     
                } else {
                    sleep(DOOR_ALTER_TIME);
                }
                pthread_mutex_unlock(&this->mutex);
                free(stopped_at);
                sleep(DOOR_ALTER_TIME);
                pthread_mutex_lock(&write_out);
                handleDoor(eid, DoorClose);
                pthread_mutex_unlock(&write_out);
                sleep(DOOR_ALTER_TIME);
            }
            pthread_mutex_lock(&this->mutex);
        }
        
        //* lock in hand ^
        //! could unlock here
        if (this->emergency_stop) {
            handleMotor(eid, MotorDown);
            pthread_mutex_unlock(&this->mutex);
            break;
        }
        //! lock again
        assert(is_empty(this->current_queue));

        //* update "DIR_NOW" list, as current is empty
        queue_t *dir_later = get_queue(this, this->direction, LATER);
        this->current_queue->first = dir_later->first;  //* point to dir_later's first ref
        dir_later->first = NULL;                        //* "reset" queue

        //* change working direction if entries waiting there
        queue_t *opposite_dir = get_queue(this, this->direction * (-1), NOW);
        if (!is_empty(opposite_dir)) {
            this->current_queue = opposite_dir;
            this->direction *= -1;
        } else printf("EID: %d Opposite direction %s queue empty, try same direction\n", eid, text(opposite_dir->type));
        
        //* no jobs available, set to idle
        if (is_empty(this->current_queue)) {
            printf("EID: %d Nothing to do!\n", eid);
            pthread_mutex_lock(&write_out);
            handleMotor(eid, MotorStop);
            this->current_queue = NULL;
            this->direction = 0;
            last_ma = MotorStop;
            pthread_mutex_unlock(&write_out);
        }
        pthread_mutex_unlock(&this->mutex);
    }
    printf("EID: %d Emergency stop\n", eid);
    return NULL;
}

//! ***************************** MAIN **********************************************************
//! ***************************** MAIN **********************************************************
//* main function, setup of elevators, listen for client events and distribute
int main(int argc, char *argv[]) {
    if (argc < 4 || (PORT = atoi(argv[3])) < 1) {
        printf("usage: ./ctrl [elevators] [hostname] [port > 0]");
        exit(-1);
    }

    ELEVATORS = atoi(argv[1]);
    HOSTNAME = argv[2];

    initHW(HOSTNAME, PORT);
    sleep(1);

    // test();
    elevator_state_t *elevator_states = (elevator_state_t*)malloc(ELEVATORS * sizeof(elevator_state_t));
    queue_t *queues = (queue_t*)malloc(4 * ELEVATORS * sizeof(queue_t)); //* 4 stop queues per elevator

    pthread_t thr[ELEVATORS];
    for (int tid = 0; tid < ELEVATORS; tid++) {
        queues[4*tid+0] = (queue_t) { .first = NULL, .type = UP_NOW };
        queues[4*tid+1] = (queue_t) { .first = NULL, .type = UP_LATER };
        queues[4*tid+2] = (queue_t) { .first = NULL, .type = DOWN_NOW };
        queues[4*tid+3] = (queue_t) { .first = NULL, .type = DOWN_LATER };

        elevator_states[tid] = (elevator_state_t) {
            .mutex = PTHREAD_MUTEX_INITIALIZER,
            .cond = PTHREAD_COND_INITIALIZER,
            .eid = tid + 1,
            .emergency_stop = 0,
            .current_position = 0.0,
            .direction = 0,
            .up_now = &queues[4*tid+0],
            .up_later = &queues[4*tid+1],
            .down_now = &queues[4*tid+2],
            .down_later = &queues[4*tid+3],
            .current_queue = NULL
        };
        pthread_create(&thr[tid], NULL, control_elevator, &elevator_states[tid]);
    }

    //* handle incoming messages
    EventDesc event_desc;
    int floor, direction, cabin, index, emergency_stops = 0;
    double position;
    while (emergency_stops < ELEVATORS) { //* until all cabins have been emergency stopped
        switch (waitForEvent(&event_desc)) {
            case FloorButton: //* find the best elevator to serve the request
                floor = event_desc.fbp.floor;
                direction = (int) event_desc.fbp.type;
                printf("-------------------------------------\n");
                printf("ACTION: Floor %d request dir %d\n", floor, direction);

                find_elevator(floor, direction, elevator_states);
                break;

            case CabinButton: //* add floor to corresponding cabin of cabinbutton 
                cabin = event_desc.cbp.cabin;
                floor = event_desc.cbp.floor;
                index = cabin - 1;

                if (elevator_states[index].direction == IDLE) {
                    printf("ACTION: Illegal, elevator %d is idle can't use cabin button!\n", cabin);
                    break;
                }

                if (floor == 32000) { //* emergency stop
                    printf("Cabin %d pressed emergency stop\n", cabin);
                    pthread_mutex_lock(&elevator_states[index].mutex);
                    elevator_states[index].emergency_stop = 1;
                    emergency_stops++;
                } else { //* add to current queue in use
                    printf("-------------------------------------\n");
                    printf("ACTION: Cabin %d requested floor %d\n", cabin, floor);
                    queue_entry_t *new_stop = (queue_entry_t*)malloc(sizeof(queue_entry_t));
                    new_stop->floor = floor;
                    new_stop->type_of_req = DEST;
                    new_stop->next = NULL;
                    pthread_mutex_lock(&elevator_states[index].mutex);
                    if (!contains(elevator_states[index].current_queue, floor))
                        enqueue_sorted(elevator_states[index].current_queue, new_stop, elevator_states[index].direction);
                    printf("Enqueued to %s ", text(elevator_states[index].current_queue->type)); 
                    printqueue(elevator_states[index].current_queue);
                    printf("-------------------------------------\n\n");
                }
                pthread_mutex_unlock(&elevator_states[index].mutex);
                pthread_cond_signal(&elevator_states[index].cond); //* elevator always sleeps here
                break;

            case Position: //* update elevator's position
                cabin = event_desc.cp.cabin;
                position = event_desc.cp.position;
                index = cabin - 1;

                pthread_mutex_lock(&elevator_states[index].mutex);
                elevator_states[index].current_position = position;
                pthread_mutex_unlock(&elevator_states[index].mutex);
                break;
            
            default:
                break;
        }
    }

    for (int tid = 0; tid < ELEVATORS; tid++)
        pthread_join(thr[tid], NULL);

    free(elevator_states);
    free(queues);
    sleep(4);
    printf("Controller shut down\n");
    return 0;  
}


//! ***************************** TEST **********************************************************
//! ***************************** TEST **********************************************************
/* test function for calculating cost */
void test() {
    int old_val = ELEVATORS;
    ELEVATORS = 3;
    elevator_state_t *elevator_states = (elevator_state_t*)malloc(3 * sizeof(elevator_state_t));

    elevator_states[0] = (elevator_state_t) {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .eid = 1,
        .emergency_stop = 0,
        .current_position = 0,
        .direction = 1,
        .up_now = NULL,
        .up_later = NULL,
        .down_now = NULL,
        .down_later = NULL,
        .current_queue = NULL
    };

    elevator_states[1] = (elevator_state_t) {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .eid = 2,
        .emergency_stop = 0,
        .current_position = 3,
        .direction = -1,
        .up_now = NULL,
        .up_later = NULL,
        .down_now = NULL,
        .down_later = NULL,
        .current_queue = NULL
    };

    elevator_states[2] = (elevator_state_t) {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .eid = 3,
        .emergency_stop = 0,
        .current_position = 1,
        .direction = -1,
        .up_now = NULL,
        .up_later = NULL,
        .down_now = NULL,
        .down_later = NULL,
        .current_queue = NULL
    };

    queue_entry_t *qe1 = (queue_entry_t*)malloc(sizeof(queue_entry_t));
    queue_entry_t *qe2 = (queue_entry_t*)malloc(sizeof(queue_entry_t));
    queue_entry_t *qe3 = (queue_entry_t*)malloc(sizeof(queue_entry_t));
    queue_entry_t *qe4 = (queue_entry_t*)malloc(sizeof(queue_entry_t));

    qe1->floor = 1;
    qe1->next = NULL;

    qe2->floor = 0;
    qe2->next = NULL;

    qe3->floor = 1;
    qe3->next = NULL;

    qe4->floor = 2;
    qe4->next = NULL;

    enqueue_sorted(elevator_states[0].up_now, qe1, 1);
    enqueue_sorted(elevator_states[0].up_now, qe2, -1);
    enqueue_sorted(elevator_states[2].up_now, qe3, 1);
    enqueue_sorted(elevator_states[2].up_now, qe4, 1);

    find_elevator(2, -1, elevator_states);

    free(elevator_states);
    ELEVATORS = old_val;
}