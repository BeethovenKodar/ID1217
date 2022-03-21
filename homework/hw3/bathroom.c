
/* includes */
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

/* mapping of names -> integers */
#define WORK 1
#define BATHROOM 0
#define WOMAN 1
#define MAN 0

/* person struct */
typedef struct person {
    int gender;
    int id;
    int visits_left;
} person_t;

/* counters - binary semaphore */
sem_t *counters;
/* men_queue/woman_queue - condition variable (binary) */
sem_t *women_queue;
sem_t *men_queue;

/* global variables, first 4 protected by counters semaphore */
int men_in_bathroom = 0;
int women_in_bathroom = 0;
int men_waiting = 0;
int women_waiting = 0;
int total_visits;
struct timeval last_reset, current_time;


/* compare last clockstamp to current to check in valid range */
int valid_time() {
    gettimeofday(&current_time, NULL);
    float elapsed = (current_time.tv_sec - last_reset.tv_sec) + 1e-6*(current_time.tv_usec - last_reset.tv_usec);
    printf("### time is %.3f ###\n", elapsed);
    if (elapsed > 0.05) return 0;
    else return 1;
}

/* resets the clock */
void reset_time() {
    gettimeofday(&last_reset, NULL);
}

/* calling thread is suspended for variable duration based on action */
void delay(int action, person_t *p) {
    char write_buffer[80];
    struct timespec interval;
    interval.tv_sec = 0;

    /* 0.1s <= bathroom delay < 0.2s, 0.3s <= work delay < 0.5s */
    if (action == BATHROOM) interval.tv_nsec = ((rand() % 100000000) + 100000000);
    if (action == WORK) interval.tv_nsec = ((rand() % 200000000) + 300000000);

    double delayed_seconds = 1e-9*(double)interval.tv_nsec;

    if (p->gender == MAN) {
        if (action == WORK) sprintf(write_buffer, "\tMan #%d works for %.3f seconds\n", p->id, delayed_seconds);
        if (action == BATHROOM) sprintf(write_buffer, "\tMan #%d uses bathroom for %.3f seconds (%d/%d)\n", \
                p->id, delayed_seconds, p->visits_left, total_visits);
    } else if (p->gender == WOMAN) {
        if (action == WORK) sprintf(write_buffer, "\tWoman #%d works for %.3f seconds\n", p->id, delayed_seconds);
        if (action == BATHROOM) sprintf(write_buffer, "\tWoman #%d uses bathroom for %.3f seconds (%d/%d)\n", \
                p->id, delayed_seconds, p->visits_left, total_visits);
    }
    write(1, write_buffer, strlen(write_buffer));
    
    nanosleep(&interval, NULL); /* don't care if not exactly delayed for interval */
}

/* entry point for men in the program */
void *man(void *person) {
    person_t *p = (person_t*)person;
    printf("### Man #%d arrived ###\n", p->id);

    while (p->visits_left > 0) {
        sem_wait(counters);
        if (women_in_bathroom > 0) { // if women in bathroom, wait in line
            men_waiting++;
            printf("\tMan #%d waiting, women inside = %d\n", p->id, women_in_bathroom);
            sem_post(counters);                
            sem_wait(men_queue);
            printf("\tMan #%d can enter, women inside = %d, men inside = %d\n", p->id, women_in_bathroom, men_in_bathroom);
            if (men_in_bathroom == 0) reset_time(); // reset time if first man entering
        } /* "pass the baton" to men waking up */

        assert(women_in_bathroom == 0);
        men_in_bathroom++;
        if (valid_time() && men_waiting > 0) { // if inside time interval and other women are waiting
            men_waiting--;
            sem_post(men_queue);
        } else {
            printf("men in bathroom = %d, time is up?\n", men_in_bathroom);
            sem_post(counters); // no men waiting, can't let women in yet
        }
        delay(BATHROOM, p);

        sem_wait(counters);
        printf("\tMan #%d done in bathroom\n", p->id);
        men_in_bathroom--;
        if (men_in_bathroom == 0 && women_waiting > 0) { // last man in bathroom
            women_waiting--;
            sem_post(women_queue);
        } else sem_post(counters); // no men waiting, anyone can enter now

        delay(WORK, p);
        p->visits_left--;
    }
    printf("### MAN %d DONE ###\n", p->id);
    return NULL;
}

/* entry point for women in the program */
void *woman(void *person) {
    person_t *p = (person_t*)person;
    printf("### Woman #%d arrived ###\n", p->id);

     while (p->visits_left > 0) {
        sem_wait(counters);
        if (men_in_bathroom > 0) { // if men in bathroom, wait in line
            women_waiting++;
            printf("\tWoman #%d waiting, men inside = %d\n", p->id, men_in_bathroom);
            sem_post(counters);                
            sem_wait(women_queue);
            printf("\tWoman #%d can enter, women inside = %d, men inside = %d\n", p->id, women_in_bathroom, men_in_bathroom);
            if (women_in_bathroom == 0) reset_time(); // reset time if first woman entering
        }  /* "pass the baton" to women waking up */
        
        assert(men_in_bathroom == 0);
        women_in_bathroom++;
        if (valid_time() && women_waiting > 0) { // if inside time interval and other men are waiting
            women_waiting--;
            sem_post(women_queue);
        } else sem_post(counters); // no women waiting, can't let men in yet

        delay(BATHROOM, p);

        sem_wait(counters);
        printf("\tWoman #%d done in bathroom\n", p->id);
        women_in_bathroom--;
        if (women_in_bathroom == 0 && men_waiting > 0) { // last woman in bathroom
            men_waiting--;
            sem_post(men_queue);
        } else {
            printf("women in bathroom = %d, time is up?\n", women_in_bathroom);
            sem_post(counters);                       // no men waiting, anyone can enter now
        }

        delay(WORK, p);
        p->visits_left--;
    }
    printf("### WOMAN %d DONE ###\n", p->id);
    return NULL;
}

int main(int argc, char *argv[]) {

    if (argc < 4) {
        printf("usage ./bath <men> <women> <bathroom visits>\n");
        exit(0);
    }

    sem_unlink("/men_q");
    sem_unlink("/women_q");
    sem_unlink("/c");

    int men = atoi(argv[1]);
    int women = atoi(argv[2]);
    total_visits = atoi(argv[3]);

    pthread_t men_thr[men];
    pthread_t women_thr[women];

    if ((men_queue = sem_open("/men_q", O_CREAT, NULL, 0)) == SEM_FAILED) exit(1);
    if ((women_queue = sem_open("/women_q", O_CREAT, NULL, 0)) == SEM_FAILED) exit(1);
    if ((counters = sem_open("/c", O_CREAT, NULL, 1)) == SEM_FAILED) exit(1);

    srand(time(NULL)); // seed random bathroom and work delays
    reset_time();

    /* create persons */
    for (int i = 0; i < men; i++) {
        person_t *p = malloc(sizeof(person_t));
        p->gender = MAN;
        p->id = i;
        p->visits_left = total_visits;
        pthread_create(&men_thr[i], NULL, man, (void*)p);
    }

    for (int i = 0; i < women; i++) {
        person_t *p = malloc(sizeof(person_t));
        p->gender = WOMAN;
        p->id = i;
        p->visits_left = total_visits;
        pthread_create(&women_thr[i], NULL, woman, (void*)p);
    }

    /* join person threads */
    for (int i = 0; i < men; i++) {
        pthread_join(men_thr[i], NULL);
    }

    for (int i = 0; i < women; i++) {
        pthread_join(women_thr[i], NULL);
    }

    sem_close(counters);
    sem_close(men_queue);
    sem_close(women_queue);
    sem_unlink("/men_q");
    sem_unlink("/women_q");
    sem_unlink("c");

    return 0;
}