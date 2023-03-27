#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "list.h"
#include "thread.h"
#include "/home/mnagabh/gem5_20.1_with_TME_NVMM_integrated/latest/gem5/new_LL_mcas_TM/concurrent-ll/include/timer.h"
#include "/home/mnagabh/gem5_20.1_with_TME_NVMM_integrated/latest/gem5/new_LL_mcas_TM/concurrent-ll/include/utils.h"
#include <unistd.h>

#define XSTR(s) STR(s)
#define STR(s) #s

// default percentage of reads
#define DEFAULT_READS 80
#define DEFAULT_UPDATES 20

// default number of threads
#define DEFAULT_NUM_THREADS 2

// default experiment duration in miliseconds
#define DEFAULT_OPERATIONS 1000000

// the maximum value the key stored in the list can take; defines the key range
#define DEFAULT_RANGE 256

//#define DEBUG 1

uint32_t operations;
int num_threads;
uint32_t finds;
uint32_t updates;
uint32_t max_key;

uint32_t op_cnt;


// per-thread seeds for the custom random function
__thread uint64_t *seeds;




/*
 * list_contains returns 1 if a node with the value val is present in the stack; else returns 0;
 */
auto llist::list_contains( val_t val)
{
    auto iterator = atomic_load(&head);
    while (iterator && iterator->data!=val) {
        iterator = iterator->next;
    }

    if(iterator && iterator->data==val)
        return 1;
    else 
        return 0;

}

/*
 * list_add inserts a new node with the given value val at the head; we can have multiple nodes in the stack with same value
 */
int llist::list_add(val_t val)
{
        auto p = make_shared<node_t>();
        p->data = val;
        p->next = atomic_load(&head);

        while(1) {
            if(p && atomic_compare_exchange_strong(&head, &(p->next), p)) {
                FAI_U32(&(size));
                return 1;
            }
                
        }

}



/*
 * list_remove removes the node at the head of the stack; 
 */
int llist::list_remove() {
     auto right = atomic_load(&head);
     while(1) {
        if(right && atomic_compare_exchange_strong(&(head), &right, right->next)) {
            FAD_U32(&(size));
            //free is done automatically because we are using shared_ptr with reference
            return 1;
        }
     }
}


int llist::list_size() {
    return size;
}



llist *the_list;


// data structure through which we send parameters to and get results from the  worker threads
typedef ALIGNED(64) struct thread_data {
    // counts the number of operations each thread performs
    unsigned long num_operations;
    // the number of elements each thread should add at the beginning of its  execution
    uint64_t num_add;
    // number of inserts a thread performs
    unsigned long num_insert;
    // number of removes a thread performs
    unsigned long num_remove;
    // number of searches a thread performs
    unsigned long num_search;
    // the id of the thread (used for thread placement on cores)
    int id;
} thread_data_t;

void test(void *data)
{
    // get the per-thread data
    int j = thread_getId();
    thread_data_t **dd = (thread_data_t **) data;
    thread_data_t *d =  *dd;
    d[j].id = j;

    // cahnge percentages of the various operations to the range 0..255 this saves a floating point operation during the benchmark  e.g instead of random()%100 to determine the next operation,  we can simply do random()&256
    uint32_t read_thresh = 256 * finds / 100;
    uint32_t rand_max;
    // seed the custom random number generator
    seeds = seed_rand();
    rand_max = max_key;
    uint32_t op;
    val_t the_value;
    int i;
    int last = -1;

    // before starting the test, we insert a number of elements in the data structure
    // we do this at each thread to avoid the situation where the entire data structure resides in the same memory node
    for (i = 0; i < d[j].num_add; ++i) {
        the_value = (val_t) my_random(&seeds[0], &seeds[1], &seeds[2]) & rand_max;
        // we make sure the insert was effective (as opposed to just updating an existing entry)
        if (the_list->list_add(the_value) == 0) {
            i--;
        }
    }

            //printf("line 118: reached here\n");
    // start the test
    while (op_cnt<operations) {
        // generate a value (node that rand_max is expected to be a power of 2)
        the_value = my_random(&seeds[0], &seeds[1], &seeds[2]) & rand_max;
        // generate the operation
        op = my_random(&seeds[0], &seeds[1], &seeds[2]) & 0xff;
        if (op < read_thresh) {
            // do a find/read operation
            the_list->list_contains(the_value);
            d[j].num_search++;
            d[j].num_operations++;
            FAI_U32(&(op_cnt));
        } else if (last == -1) {
            // do a write i.e add or remove operation
            if (the_list->list_add(the_value)) {
                d[j].num_insert++;
                d[j].num_operations++;
                last = 1;
                FAI_U32(&(op_cnt));
            }
        } else {
            // do a delete operation
            if (the_list->list_remove()) {
                d[j].num_remove++;
                d[j].num_operations++;
                last = -1;
                FAI_U32(&(op_cnt));
            }
        }
    }
    return ;
}

int main(int argc, char *const argv[])
{
    //int numProcs = (int) sysconf(_SC_NPROCESSORS_ONLN);

    thread_data_t *data;

    //num_threads = numProcs - 1;
    //if ((num_threads%2)==1) {
    //    num_threads--;
    //}
    //num_threads = 4;
    num_threads = atoi(argv[1]);
    max_key = DEFAULT_RANGE;
    updates = DEFAULT_UPDATES;
    finds = DEFAULT_READS;
    //operations = DEFAULT_OPERATIONS;
    operations = atoi(argv[2]);
    op_cnt = 0;

    
    max_key--;
    // we round the max key up to the nearest power of 2, which makes our random key generation more efficient
    max_key = pow2roundup(max_key) - 1;

    // initialization of the list
    the_list = new llist();

    // initialize the data which will be passed to the threads
    if ((data = (thread_data_t*)malloc(num_threads * sizeof(thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    printf("Initial parameters\n");
    printf("No. threads %d\n",num_threads);
    printf("Max key range %d\n",max_key);
    printf("No. Ops %d\n",operations);
    printf("Op Cnt %d\n",op_cnt);

    thread_startup(num_threads);
    
    // set the data for each thread and create the threads
    for (int i = 0; i < num_threads; i++) {
        data[i].id = i;
        data[i].num_operations = 0;
        data[i].num_insert = 0;
        data[i].num_remove = 0;
        data[i].num_search = 0;
        data[i].num_add = max_key / (2 * num_threads);
        if (i < ((max_key / 2) % num_threads))
            data[i].num_add++;
    }
    
    TIMER_T startTime;
    TIMER_READ(startTime);
    thread_start(test, (void*)&data);
    TIMER_T stopTime;
    TIMER_READ(stopTime);

    //test((void*) &data);

    // report some experiment statistics
    printf("Elapsed time    = %f seconds\n", TIMER_DIFF_SECONDS(startTime, stopTime));
    uint32_t operations_final = 0;
    long reported_total = 0;
    for (int i = 0; i < num_threads; i++) {
        printf("Thread %d\n", data[i].id);
        printf("  #operations   : %lu\n", data[i].num_operations);
        printf("  #inserts   : %lu\n", data[i].num_insert);
        printf("  #removes   : %lu\n", data[i].num_remove);
        printf("  #searches   : %lu\n", data[i].num_search);
        printf("  #adds   : %lu\n", data[i].num_add);
        operations_final += data[i].num_operations;
        reported_total = reported_total + data[i].num_add + data[i].num_insert -
                         data[i].num_remove;
    }
    printf("final Op cnt : global %d and local_add %d\n",op_cnt,operations_final);
    printf("Expected size: %ld Actual size: %d\n", reported_total,the_list->list_size());


    thread_shutdown();
    free(data);

    return 0;
}
