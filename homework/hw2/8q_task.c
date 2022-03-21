
#ifndef _REENTRANT
    #define _REENTRANT
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <omp.h>

/* LOCALLY:     gcc -Xpreprocessor -fopenmp -lomp omp8q.c -o omp8q */
/* SUBWAY:      gcc -std=c99 -fopenmp omp8q.c -o omp8q */

typedef struct print_buf {
    int top;
    int qpositions[100][8];
} print_buf;

typedef struct qcombination {
    int top;
    int qpositions[];
} qcombination;

print_buf printouts = { 0, {{0}} };
qcombination *qcomb;
long productions = 0;
long consumptions = 0;
int N;  /* NxN board and N queens to place */
int threads; /* threads to use */


/* ##########################################################################################
   ################################## VALIDATION FUNCTIONS ##################################
   ########################################################################################## */

/* Validate that no queens are placed on same row */
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

/* Validate that no queens are placed in same column */
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

/* Validate that no queens are placed on same left and right diagonals */
int valid_diagonals(int qpositions[]) {
    int left_bottom_diagonals[N];
    int right_bottom_diagonals[N];
    int row, col, temp_col, temp_row, fill_value, index;

    for (int queen = 0; queen < N; queen++) {
        row = qpositions[queen] / N;
        col = qpositions[queen] % N;
        
        /* position --> left down diagonal endpoint, i.e. index */
        fill_value = col < row ? col : row; // closest to bottom or left wall
        temp_row = row - fill_value;
        temp_col = col - fill_value;
        index = temp_row * N + temp_col;
        /* check if interference occurs */
        for (int i = 0; i < queen; i++) {
            if (left_bottom_diagonals[i] == index) return 0;
        }
        left_bottom_diagonals[queen] = index; // no interference

        /* position --> right down diagonal endpoint, i.e. index) */
        fill_value = (N-1) - col < row ? N - col - 1 : row; // closest to bottom or right wall
        temp_row = row - fill_value;
        temp_col = col + fill_value;
        index = temp_row * N + temp_col;
        /* check if interference occurs */
        for (int i = 0; i < queen; i++) { 
            if (right_bottom_diagonals[i] == index) return 0;
        }
        right_bottom_diagonals[queen] = index; // no interference
    }
    return 1;
}

/* ##########################################################################################
   #################################### HELPER FUNCTIONS ####################################
   ########################################################################################## */

/* print the collected solutions  */
void print(print_buf printouts) {
    static int solution_number = 1;
    int placement;

    printf("\n");
    /* iterate over all solutions collected */
    for (int sol = 0; sol < printouts.top; sol++) {
        /* print physical positions in a list */
        printf("Solution %d: [ ", solution_number++);
        for (int pos = 0; pos < N; pos++) {
            printf("%d ", printouts.qpositions[sol][pos]+1);
        } 
        printf("]\n");

        /* print the board and placed queens */
        printf("Placement:\n");
        for (int i = 1; i <= N; i++) { // rows
            printf("[ ");
            placement = printouts.qpositions[sol][N-i];
            for (int j = (N-i)*N; j < (N-i)*N+N; j++) {
                if (j == placement) {
                    printf(" Q ");
                } else printf("%2d ", j+1);
            }
            printf("]\n");
        }
        printf("\n");
    }
}


/* ##########################################################################################
   #################################### THREAD FUNCTIONS ####################################
   ########################################################################################## */

/* evaluates a placement of n queens */
void eval_positioning(int qpositions[]) {
    #pragma omp atomic
        consumptions++;
    if (valid_rows(qpositions) && valid_columns(qpositions) && valid_diagonals(qpositions)) {
        /* save for printing later */
        int top;
        #pragma omp critical
            top = printouts.top++;
        memcpy(printouts.qpositions[top], qpositions, N*sizeof(int));  
    }
}

/* recursively generate all possible queen combinations
   binomial coefficient, without order without replacement
   8 queens on 8x8 board: 4'426'165'368 combinations */
void generate_positions(int pos, int queens) {
    static int top = 0;
    static int qpositions[8];

    if (queens == 0) { // base case
        #pragma omp atomic 
            productions++;
        #pragma omp task
            eval_positioning(qpositions);
        #pragma omp taskwait
        return;
    }

    for (int i = pos; i <= N*N - queens; i++) {
        qpositions[top++] = i;
        generate_positions(i+1, queens-1);
        top--;
    }
}


/* ##########################################################################################
   ########################################## MAIN ##########################################
   ########################################################################################## */

int main(int argc, char *argv[]) {
    
    if (argc < 3) {
        printf("usage: ./8q <threads> <board width/height>\n");
        exit(1);
    }

    N = atoi(argv[2]);
    if (N < 2 || N > 8) {
        printf("Wrong input! 2 <= N <= 8\n");
        return 0;
    }
    threads = atoi(argv[1]);
    struct timeval start, stop;
    double elapsed;
    
    gettimeofday(&start, NULL);

    // omp_set_dynamic(0);
    omp_set_num_threads(threads);
    omp_set_max_active_levels(1);
    #pragma omp parallel 
    {
        #pragma omp single
            generate_positions(0, N);
    }
    
    gettimeofday(&stop, NULL);

    elapsed = stop.tv_sec - start.tv_sec;
    elapsed += (stop.tv_usec - start.tv_usec) / (double)1000000;
    
    /* print the solutions to the N-queen problem */
    print(printouts);
    
    printf("board: %dx%d, threads: %d, exec time: %fs, solutions: %d\n", N, N, threads, elapsed, printouts.top);
    printf("productions:  %ld\nconsumptions: %ld\n", productions, consumptions);

    printf("\n8x8: 4426165368 \t 92\n7x7: 85900584 \t\t 40\n6x6: 1947792 \t\t 4\n5x5: 53130 \t\t 10\n");
    return 0;
}