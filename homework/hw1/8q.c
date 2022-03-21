
#ifndef _REENTRANT
    #define _REENTRANT
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>


typedef struct stack_buf {
    int positions[8];
    int top;
} stack_buf;

typedef struct global_buf {
    int positions[8];
    int buf_empty;
    long prod_done;
} global_buf;

typedef struct print_buf {
    int qpositions[100][8];
    int top;
} print_buf;

stack_buf queen_comb = { {0}, 0 };
print_buf printouts = { {{0}}, 0 };
global_buf global = { {0}, 1, 0 };
int N; //NxN board and N queens to place
long productions = 0;
long consumptions = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t filled = PTHREAD_COND_INITIALIZER;


/* ##########################################################################################
   ################################## VALIDATION FUNCTIONS ##################################
   ########################################################################################## */

/* Validate that no queens are placed on the same row */
int valid_rows(int qpositions[]) {
    int rows[N];
    memset(rows, 0, N*sizeof(int));
    int row;
    for (int i = 0; i < N; i++) {
        row = qpositions[i] / N;
        if (rows[row] == 0) rows[row] = 1;
        else return 0;
    }
    return 1;
}

/* Validate that no queens are placed in the same column */
int valid_columns(int qpositions[]) {
    int columns[N];
    memset(columns, 0, N*sizeof(int));
    int column;
    for (int i = 0; i < N; i++) {
        column = qpositions[i] % N;
        if (columns[column] == 0) columns[column] = 1;
        else return 0;
    }
    return 1;
}

/* Validate that left and right diagonals aren't used by another queen */
int valid_diagonals(int qpositions[]) {
    int left_bottom_diagonals[N];
    int right_bottom_diagonals[N];
    int row, col, temp_col, temp_row, fill_value, index;

    for (int queen = 0; queen < N; queen++) {
        row = qpositions[queen] / N;
        col = qpositions[queen] % N;
        
        /* position --> left down diagonal endpoint (index) */
        fill_value = col < row ? col : row; // closest to bottom or left wall
        temp_row = row - fill_value;
        temp_col = col - fill_value;
        index = temp_row * N + temp_col; // board position
        for (int i = 0; i < queen; i++) { // check if interference occurs
            if (left_bottom_diagonals[i] == index) return 0;
        }
        left_bottom_diagonals[queen] = index; // no interference

        /* position --> right down diagonal endpoint (index) */
        fill_value = (N-1) - col < row ? N - col - 1 : row; // closest to bottom or right wall
        temp_row = row - fill_value;
        temp_col = col + fill_value;
        index = temp_row * N + temp_col; // board position
        for (int i = 0; i < queen; i++) { // check if interference occurs
            if (right_bottom_diagonals[i] == index) return 0;
        }
        right_bottom_diagonals[queen] = index; // no interference
    }
    return 1;
}

/* ##########################################################################################
   #################################### HELPER FUNCTION(S) ####################################
   ########################################################################################## */

/* print the collected solutions  */
void print(print_buf printouts) {
    static int solution_number = 1;
    int placement;

    pthread_mutex_lock(&print_mutex);
    for (int sol = 0; sol < printouts.top; sol++) { // all solutions
        printf("Solution %d: [ ", solution_number++);
        for (int pos = 0; pos < N; pos++) {
            printf("%d ", printouts.qpositions[sol][pos]+1);
        } 
        printf("]\n");

        printf("Placement:\n");
        for (int i = 1; i <= N; i++) { // rows
            printf("[ ");
            placement = printouts.qpositions[sol][N-i];
            for (int j = (N-i)*N; j < (N-i)*N+N; j++) { // physical position
                if (j == placement) {
                    printf(" Q ");
                } else printf("%2d ", j+1);
            }
            printf("]\n");
        }
        printf("\n");
    }
    pthread_mutex_unlock(&print_mutex);
}


/* ##########################################################################################
   #################################### THREAD FUNCTIONS ####################################
   ########################################################################################## */

/* entry point for each worker (consumer) workers will 
check each queen's row, column and diagonal to evaluate 
satisfactory placements */
void *eval_positioning(void *id) {
    long thr_id = (long)id;
    int qpositions[N]; // on stack (thread-private)
    
    while (1) {
        pthread_mutex_lock(&buffer_mutex);
        while (global.buf_empty && !global.prod_done) // no element ready and not done generating
            pthread_cond_wait(&filled, &buffer_mutex);

        if (global.prod_done && global.buf_empty) { // no element ready and done generating
            pthread_cond_signal(&filled);
            break;
        } else if (!global.buf_empty) {
            memcpy(qpositions, global.positions, N*sizeof(int)); // copy to local scope
            consumptions++;

            global.buf_empty = 1;
            pthread_mutex_unlock(&buffer_mutex);
            pthread_cond_signal(&empty);

            if (valid_rows(qpositions) && valid_columns(qpositions) && valid_diagonals(qpositions)) {
                pthread_mutex_lock(&print_mutex);
                int top = printouts.top++;
                pthread_mutex_unlock(&print_mutex);
                // thread has its own index in printouts now
                memcpy(printouts.qpositions[top], qpositions, N*sizeof(int));
            }
        } else {
            pthread_mutex_unlock(&buffer_mutex);
        }
    }
    pthread_mutex_unlock(&buffer_mutex);
    return NULL;
}

/* recursively generate all possible queen_combs */
void rec_positions(int pos, int queens) {
    if (queens == 0) { // base case
        pthread_mutex_lock(&buffer_mutex);
        while (global.buf_empty == 0) { // while production hasn't been consumed
            pthread_cond_wait(&empty, &buffer_mutex);
        }
        memcpy(global.positions, queen_comb.positions, N*sizeof(int));
        productions++;
        global.buf_empty = 0;
        pthread_mutex_unlock(&buffer_mutex);
        pthread_cond_signal(&filled);
        return;
    }

    for (int i = pos; i <= N*N - queens; i++) {
        queen_comb.positions[queen_comb.top++] = i;
        rec_positions(i+1, queens-1);
        queen_comb.top--;
    }
}

/* binomial coefficient | without order, without replacement
8 queens on 8x8 board: 4'426'165'368 queen combinations */
void *generate_positions(void *arg) {
    rec_positions(0, N);
    pthread_mutex_lock(&buffer_mutex);
    global.prod_done = 1;
    pthread_mutex_unlock(&buffer_mutex);
    pthread_cond_broadcast(&filled); //wake all to 
    return NULL;
}

/* ##########################################################################################
   ########################################## MAIN ##########################################
   ########################################################################################## */

/* main procedure of the program */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: ./8q <workers> <board width/height>\n");
        exit(1);
    }

    int workers = atoi(argv[1]);
    N = atoi(argv[2]);

    if (N < 2 || N > 8) {
        printf("Wrong input! 2 <= N <= 8\n");
        return 0;
    }

    struct timeval start, stop;
    double elapsed;
    pthread_t consumers[workers];
    pthread_t producer;

    printf("\n");
    
    gettimeofday(&start, NULL);

    pthread_create(&producer, NULL, generate_positions, NULL);
    for (long i = 0; i < workers; i++) {
        pthread_create(&consumers[i], NULL, eval_positioning, (void*)i+1);
    }

    pthread_join(producer, NULL);
    for (int i = 0; i < workers; i++) {
        pthread_join(consumers[i], NULL);
        char id[2];
        sprintf(id, "%d", i+1);
        write(1, id, strlen(id));
        write(1, " done\n\n", 6);
    }

    gettimeofday(&stop, NULL);
    elapsed = stop.tv_sec - start.tv_sec;
    elapsed += (stop.tv_usec - start.tv_usec) / (double)1000000;
    
    /* go through all valid solutions and print */
    print(printouts);
    
    printf("board: %dx%d, workers: %d (+1), exec time: %fs, solutions: %d\n", N, N, workers, elapsed, printouts.top);
    printf("productions:  %ld\nconsumptions: %ld\n", productions, consumptions);
    return 0;
}