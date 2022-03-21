
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef enum gender {
    MAN,
    WOMAN
} gender_t;

/* men use array of women and fill in each womens rating_t, and vice versa */
typedef struct rating {
    gender_t gender;
    int id;
    int rating;
} rating_t;

const int OK = 1;
const int NOT_OK = 0;

/*******************************************************************************************
 * HELPER FUNCTIONS ************************************************************************
 *******************************************************************************************/

/* custom compare for qsort */
int compare(const void *r1, const void *r2) {
    return ((rating_t*)r1)->rating > ((rating_t*)r2)->rating ? -1 : 1;
}

/* random shuffling of ratings */
void shuffle_ratings(rating_t *profiles, int size) {
    int random_index, temp;
    for (int max_index = size-1; max_index > 0; max_index--) {
        random_index = rand() % (max_index+1);
        /* swap values at indexes */
        temp = profiles[max_index].rating;
        profiles[max_index].rating = profiles[random_index].rating;
        profiles[random_index].rating = temp;
    }
}

/*******************************************************************************************
 * PROCESSES *******************************************************************************
 *******************************************************************************************/ 

/* keeps track of women who are with a man, eventually notifies them it's done */
void monitor_proposals(int people_per_gender) {
    MPI_Status status;
    
    int *women_atleast_one_proposal = (int*)calloc(people_per_gender, sizeof(int));
    int satisfied_women = 0;
    int sender, index;
    int receive_buf;

    while (satisfied_women < people_per_gender) {
        MPI_Recv(&receive_buf, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        sender = status.MPI_SOURCE;
        index = sender/2;

        if (women_atleast_one_proposal[index] == 0) {
            women_atleast_one_proposal[index] = sender + 1; /* logical id */
            satisfied_women++;
            printf("Monitor: First proposal to woman (%d)\n", sender+1);
            fflush(stdout);
        }

        if (satisfied_women == people_per_gender) {
            int dest;
            for (int i = 0; i < people_per_gender; i++) {
                dest = women_atleast_one_proposal[i] - 1;
                MPI_Send(&OK, 1, MPI_INT, dest, dest, MPI_COMM_WORLD);
                fflush(stdout);
            }
            printf("Monitor: All women have found a partner, quit\n");
            fflush(stdout);
        }
    }
}

/* function for men, highest rating is proposed to first */
void propose(int id, rating_t *my_ratings) {
    MPI_Status status;
    int proposals = 0;
    int accepted = 0;
    int propose_dest, propose_rating;

    while (!accepted) {
        propose_dest = my_ratings[proposals].id - 1;
        propose_rating = my_ratings[proposals].rating;
        // propose_rating = 69;
        printf("Man (%d): Proposed to woman (%d) who's rated %d\n", id, propose_dest+1, propose_rating);
        fflush(stdout);
        MPI_Send(&propose_rating, 1, MPI_INT, propose_dest, propose_dest, MPI_COMM_WORLD);
        proposals++;
        MPI_Recv(&accepted, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        printf("Man (%d): Denied by Woman (%d)\n", id, propose_dest);
        fflush(stdout);
    }
    printf("Man (%d): Accepted!\n", id);
    fflush(stdout);
}

/* function for women, accepts first proposal but can replace */
void receive_proposals(int id, rating_t *my_ratings, int monitor_rank) {
    MPI_Status status;

    int DONT_CARE = 0;
    int receive_buf = -1;
    int received_man_sent_rating, received_man_rating, received_man_rank;
    int best_man_sent_rating, best_man_rating, best_man_rank = -1;

    while (1) {
        MPI_Recv(&receive_buf, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        /* check if message was from the monitor or a man */
        if (status.MPI_SOURCE == monitor_rank && receive_buf == OK) break;
        else {
            received_man_sent_rating = receive_buf;
            received_man_rank = status.MPI_SOURCE;
            received_man_rating = my_ratings[received_man_rank/2].rating;
            
            if (best_man_rank == -1) { /* first proposal received */
                best_man_sent_rating = received_man_sent_rating;
                best_man_rank = received_man_rank;
                best_man_rating = received_man_rating;
                MPI_Send(&DONT_CARE, 1, MPI_INT, monitor_rank, monitor_rank, MPI_COMM_WORLD);
                printf("Woman (%d): Temporarily accepted man (%d, #%d#)\n", id, best_man_rank+1, best_man_rating);
                fflush(stdout);

            } else if (received_man_rating > best_man_rating) { /* proposal is better rated than current accepted, notify replaced */
                MPI_Send(&NOT_OK, 1, MPI_INT, best_man_rank, best_man_rank, MPI_COMM_WORLD);
                printf("Woman (%d): Replaced man (%d, #%d#) for man (%d, #%d#)\n", id, best_man_rank+1, \
                        best_man_rating, received_man_rank+1, received_man_rating);
                fflush(stdout);
                best_man_sent_rating = received_man_sent_rating;
                best_man_rank = received_man_rank;
                best_man_rating = received_man_rating;

            } else { /* notify denied man */
                MPI_Send(&NOT_OK, 1, MPI_INT, received_man_rank, received_man_rank, MPI_COMM_WORLD);
                printf("Woman (%d): Rejected proposing man (%d, #%d#) due to best man (%d, #%d#)\n", id, received_man_rank+1, \
                        received_man_rating, best_man_rank+1, best_man_rating);
                fflush(stdout);
            }
        }
    }

    /* send ok to accepted man */
    printf("Woman (%d): Decided to marry man (%d)\n", id, best_man_rank+1);
    fflush(stdout);
    MPI_Send(&OK, 1, MPI_INT, best_man_rank, best_man_rank, MPI_COMM_WORLD);
    printf("## Married: Woman (%d, #%d#) + Man (%d, #%d#)\n", id, best_man_sent_rating, best_man_rank+1, best_man_rating);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int pool_size, people_per_gender;

    int rank, id, monitor_rank;
    rating_t *my_ratings;
    gender_t gender;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &pool_size);
    
    if (pool_size % 2 != 1) {
        if (rank == 0) 
            printf("Requirement: men == women and 1 extra process!\n");
        MPI_Finalize();
        exit(1);
    }

    people_per_gender = pool_size / 2;                                          /* number of men/women */
    id = rank + 1;                                                              /* logical id */
    monitor_rank = pool_size - 1;                                               /* collecting of proposals */

    if (rank != monitor_rank) {
        gender = (id % 2 == 0 ? MAN : WOMAN);                                   /* odd id - woman, even id - man */
        my_ratings = (rating_t*)malloc(people_per_gender * sizeof(rating_t));   /* rate half of pool, i.e. other gender */

        /* create "profiles" of other gender */
        for (int i = 0; i < people_per_gender; i++) {
            my_ratings[i].gender = (gender == MAN ? WOMAN : MAN);
            my_ratings[i].id = ( gender == MAN ? (2*i+1) : (2*i+2) );
            my_ratings[i].rating = i+1;
        }

        /* randomize ratings of other gender */
        srand(time(NULL) + id);
        shuffle_ratings(my_ratings, people_per_gender);
        qsort(my_ratings, people_per_gender, sizeof(rating_t), compare);

        if (gender == WOMAN)    printf("W(%d) ratings: ", id);
        else if (gender == MAN) printf("M(%d) ratings: ", id);

        for (int i = 0; i < people_per_gender; i++) 
            printf("| {id:%d, %d} | ", my_ratings[i].id, my_ratings[i].rating);
        printf("\n");
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == monitor_rank) printf("\n");
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);

    /* call function based on process type */
    if (rank == monitor_rank) {
        monitor_proposals(people_per_gender);
    } else {
        if (gender == WOMAN) 
            receive_proposals(id, my_ratings, monitor_rank);
        else if (gender == MAN)
            propose(id, my_ratings);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    printf("ID (%d): Done\n", id);

    MPI_Finalize();
    return 0;
}
