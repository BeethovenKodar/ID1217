
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define BENCHES 5

typedef struct idx {
    int row, col, val;
} idx;

/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {

    if (argc < 6) {
        printf("usage ./matrix <size1> <size2> <size3> <min_threads> <max_threads>\n");
        exit(1);
    }
    /* read command line args if any */
    int sizes[3];
    for (int i = 1; i < 4; i++)
        sizes[i-1] = atoi(argv[i]);

    int min_threads = atoi(argv[4]);
    int max_threads = atoi(argv[5]);

    /* benchmark three different matrix sizes */
    for (int loop = 0; loop < 3; loop++) {
        int size = sizes[loop];
        int matrix[size][size];
        double seq_time; 

        /* initialize (and print) the matrix */
        
        /* set random values in array */
        srand(time(NULL));
        for (int i = 0; i < size; i++) {
            // printf("[");
            for (int j = 0; j < size; j++) {
                matrix[i][j] = rand() % (size*size*size);
                // printf(" %3d", matrix[i][j]);
            }
            // printf(" ]\n");
        }
 
        printf("\nNOTE: same matrix used for each run, min/max indices might differ between runs because of multiple occurences\n");
        printf("SIZE | THREADS | EXEC TIME | SPEEDUP vs min_threads | SUM | MIN | MAX\n");

        /* benchmark with different number of threads */
        for (int threads = min_threads; threads <= max_threads; threads++) {
            double start_time, end_time, exec_time_avg, speedup, acc_time = 0.0;
            /* 10 benchmarks on same matrix and number of threads */
            for (int bench = 0; bench < BENCHES; bench++) { 
                long total = 0;
                int i, j, val = 0;
                struct idx min = {-1, -1, size*size*size};
                struct idx max = {-1, -1, -1};

                omp_set_num_threads(threads);
                start_time = omp_get_wtime();

                #pragma omp parallel for reduction(+:total) private(i, j, val)
                for (i = 0; i < size; i++) {        /* rows */
                    for (j = 0; j < size; j++) {    /* columns */
                        val = matrix[i][j];
                        total += val;
                        if (val > max.val)     //reduction (min : min.val) etc. possible
                            #pragma omp critical
                                if (val > max.val) {
                                    max.row = size - i;
                                    max.col = j+1;
                                    max.val = val;
                            }
                        if (val < min.val)     //reduction (max : max.val) etc. possible
                            #pragma omp critical
                                if (val < min.val) {
                                    min.row = size - i;
                                    min.col = j+1;
                                    min.val = val;
                                }
                    }
                } /* implicit barrier */

                end_time = omp_get_wtime();
                acc_time += (end_time - start_time);

                if (bench == (BENCHES - 1)) {
                    exec_time_avg = acc_time/BENCHES;
                    if (threads == min_threads) seq_time = exec_time_avg;
                    speedup = seq_time/exec_time_avg;
                    printf("| %d | %d | %f | %f | %ld | %2d at [%3d][%3d] | %2d at [%3d][%3d] |\n",
                    size, threads, exec_time_avg, speedup, total, min.val, min.row, min.col, max.val, max.row, max.col);
                }
            }
        }
    }
    printf("\n");
}