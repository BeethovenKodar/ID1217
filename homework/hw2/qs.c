
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define BENCHES 5
#define SEQ_REGION 10

/* utility to check if list is sorted */
int isSorted(int *a, int size) {
   for (int i = 0; i < size-1; i++)
      if (a[i] > a[i+1])
        return 0;
   return 1;
}

/* utility to swap value of two memory locations with integers */
void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/* determines pivot and places element on left of right side of pivot based on value */
int partition(int arr[], int low, int high) {
    int l = low;                            /* initial pivot index */
    int h = high;
    while (l != h) {                        /* find elements smaller than arr[low] */
        if (arr[l] <= arr[h]) {             /* larger element found, let pivot remain */
            h--;
        } else if (arr[l+1] <= arr[l]) {
            swap(&arr[l+1], &arr[l]);       /* found smaller element, move pivot up */
            l++;
        } else {
            swap(&arr[l+1], &arr[h]);
        }
    }
    return l;                               /* l is now the new pivot index */
}

/* recursive quicksort, split arr in two index ranges and divide work */
void qs(int arr[], int low, int high) {
    if (low < high) {
        int pivot = partition(arr, low, high);
        #pragma omp task shared(arr) if(high - low > SEQ_REGION)
        {
            qs(arr, low, pivot-1);
        }
        #pragma omp task shared(arr) if(high - low > SEQ_REGION)
        {
            qs(arr, pivot+1, high);
        }
        #pragma omp taskwait
    }
}

/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("usage ./qs <length1> <length2> <length3> <min_threads> <max_threads>\n");
        exit(1);
    }

    int lengths[3];
    for (int i = 1; i < 4; i++)
        lengths[i-1] = atoi(argv[i]);

    int min_threads = atoi(argv[4]);
    int max_threads = atoi(argv[5]);

    /* benchmark three different matrix sizes */
    for (int loop = 0; loop < 3; loop++) {
        int list_length = lengths[loop];
        double seq_time;

        printf("%10s \t%7s EXEC TIME \t SPEEDUP\n", "LENGTH", "THREADS");
        /* benchmark with different number of threads */
        for (int threads = min_threads; threads <= max_threads; threads++) {
            /* allocated unsorted list on heap */
            int *unsorted = (int*)malloc((list_length) * sizeof(int));
            double start_time, end_time, exec_time_avg, speedup, acc_time = 0.0;

            /* 10 benchmarks on same unsorted list and number of threads */
            for (int bench = 0; bench < BENCHES; bench++) {

                /* Initialize array with random elements */
                srand(time(NULL));
                for (int i = 0; i < list_length; i++) {
                    unsorted[i] = (rand() % list_length*2);
                }

                start_time = omp_get_wtime();
                omp_set_dynamic(0);
                omp_set_num_threads(threads);
                #pragma omp parallel
                {
                    #pragma omp single
                        qs(unsorted, 0, list_length-1);
                }
                end_time = omp_get_wtime();
                acc_time += (end_time - start_time);

                assert(isSorted(unsorted, list_length) == 1);

                if (bench == (BENCHES - 1)) {
                    exec_time_avg = acc_time/BENCHES;
                    if (threads == min_threads) seq_time = exec_time_avg;
                    speedup = seq_time/exec_time_avg;
                    printf("%10d %7d \t%f\t%f\n", list_length, threads, exec_time_avg, speedup);
                }
            }
            free(unsorted);
        }
        printf("\n----------------------------------------------------\n\n");
    }

}