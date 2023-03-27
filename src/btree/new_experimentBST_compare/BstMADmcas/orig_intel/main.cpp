#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>

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
#define DEFAULT_OPERATIONS 10000000

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



static size_t compareAndExchange( volatile size_t* addr, size_t oldval, size_t newval ){
    size_t ret;
    __asm__ volatile( "lock cmpxchg %2, %1\n\t":"=a"(ret), "+m"(*addr): "r"(newval), "0"(oldval): "memory" );
    return ret;
}
static size_t compareAndExchangeTid( volatile size_t* addr, int oldval, int newval ){
    int ret;
    __asm__ volatile( "lock cmpxchg %2, %1\n\t":"=a"(ret), "+m"(*addr): "r"(newval), "0"(oldval): "memory" );
    return ret;
}
static size_t AtomicExchange(volatile size_t* ptr, size_t new_value) {
    __asm__ __volatile__("xchg %1,%0": "=r" (new_value): "m" (*ptr), "0" (new_value): "memory");
    return new_value; // Now it’s the previous value.
}
template<typename T,unsigned N=sizeof (uint32_t)> struct DPointer {
    public:
        union {
            uint64_t ui;
            struct {
                T* ptr;
                size_t mark;
            };
        };
        DPointer() : ptr(NULL), mark(0) {}
        DPointer(T* p) : ptr(p), mark(0) {}
        DPointer(T* p, size_t c) : ptr(p), mark(c) {}
        bool cas(DPointer<T,N> const& nval, DPointer<T,N> const & cmp) {
            bool result;
            __asm__ __volatile__(
            "lock cmpxchg8b %1\n\t"
            "setz %0\n"
            : "=q" (result)
            ,"+m" (ui)
            : "a" (cmp.ptr), "d" (cmp.mark)
            ,"b" (nval.ptr), "c" (nval.mark)
            : "cc"
            );
            return result;
        }
        // We need == to work properly
        bool operator==(DPointer<T,N> const&x) { return x.ui == ui; }
};


    template<typename T> struct DPointer <T,sizeof (uint64_t)> {
        public:
            union {
                uint64_t ui[2];
                struct {
                    T* ptr;
                    size_t mark;
                } __attribute__ (( __aligned__( 16 ) ));
            };
            DPointer() : ptr(NULL), mark(0) {}
            DPointer(T* p) : ptr(p), mark(0) {}
            DPointer(T* p, size_t c) : ptr(p), mark(c) {}
            bool cas(DPointer<T,8> const& nval, DPointer<T,8> const& cmp)   {
                bool result;
                __asm__ __volatile__ (
                "lock cmpxchg16b %1\n\t"
                "setz %0\n"
                : "=q" ( result )
                ,"+m" ( ui )
                : "a" ( cmp.ptr ), "d" ( cmp.mark )
                ,"b" ( nval.ptr ), "c" ( nval.mark )
                : "cc"
                );
                return result;
            }
        // We need == to work properly
        bool operator==(DPointer<T,8> const&x) { return x.ptr == ptr && x.mark == mark; }
    };




class Node {
    public:
        long key;
        long value;
        DPointer<Node, sizeof(size_t)> lChild;
        DPointer<Node, sizeof(size_t)> rChild;
        Node() {
        }
        Node(long key, long value) {
            this->key = key;
            this->value = value;
        }
        Node(long key, long value, DPointer<Node, sizeof(size_t)> lChild, DPointer<Node, sizeof(size_t)> rChild) {
            this->key = key;
            this->value = value;
            this->lChild = lChild;
            this->rChild = rChild;
        }
};


class SeekRecord {
    public:
        Node *ancestor;
        Node *successor;
        Node *parent;
        Node *leaf;
        SeekRecord() {
        }
        SeekRecord(Node *ancestor, Node *successor, Node *parent, Node *leaf) {
            this->ancestor = ancestor;
            this->successor = successor;
            this->parent = parent;
            this->leaf = leaf;
        }
};
class LockFreeBST {
    public:
    static Node *grandParentHead;
    static Node *parentHead;
    static LockFreeBST *obj;
    LockFreeBST() {
        createHeadNodes();
    }
    long lookup(long target) {
        Node *node = grandParentHead;
        while (node->lChild.ptr != NULL) //loop until a leaf or dummy node is reached
        {
            if (target < node->key) {
                node = node->lChild.ptr;
            } else {
                node = node->rChild.ptr;
            }
        }
        if (target == node->key)
            return (1);
        else
            return (0);
    }
    
    void add(long insertKey) {
        int nthChild;
        Node *node;
        Node *pnode;
        SeekRecord *s;
        while (true) {
            nthChild = -1;
            pnode = parentHead;
            node = parentHead->lChild.ptr;
            while (node->lChild.ptr != NULL) //loop until a leaf or dummy node is reached
            {
                if (insertKey < node->key) {
                    pnode = node;
                    node = node->lChild.ptr;
                } else {
                    pnode = node;
                    node = node->rChild.ptr;
                }
            }
            Node *oldChild = node;
            if (insertKey < pnode->key) {
                nthChild = 0;
            } else {
                nthChild = 1;
            }
            //leaf node is reached
            if (node->key == insertKey) {
                //key is already present in tree. So return
                return;
            }
            Node *internalNode, *lLeafNode, *rLeafNode;
            if (node->key < insertKey) {
                rLeafNode = new Node(insertKey, insertKey);
                internalNode = new Node(insertKey, insertKey, DPointer<Node, sizeof(size_t)>(node,0), DPointer<Node, sizeof(size_t)>(rLeafNode, 0));
            } else {
                lLeafNode = new Node(insertKey, insertKey);
                internalNode = new Node(node->key, node->key, DPointer<Node, sizeof(size_t)>(lLeafNode, 0), DPointer<Node, sizeof(size_t)>(node, 0));
            }
            if (nthChild == 0) {
                if (pnode->lChild.cas(DPointer<Node, sizeof(size_t)>(internalNode, 0), oldChild)) {
                    return;
                } else {
                    //insert failed; help the conflicting delete operation
                    if (node == pnode->lChild.ptr) { // address has not changed. So CAS would have failed coz of flag/mark only
                        //help other thread with cleanup
                        s = seek(insertKey);
                        cleanUp(insertKey, s);
                    }
                }
            } else {
                if (pnode->rChild.cas(DPointer<Node, sizeof(size_t)>(internalNode, 0), DPointer<Node,sizeof(size_t)>(oldChild, 0))) {
                    return;
                } else {
                    if (node == pnode->rChild.ptr) {
                        s = seek(insertKey);
                        cleanUp(insertKey, s);
                    }
                }
            }
        }
    }

    void remove(long deleteKey) {
        bool isCleanUp = false;
        SeekRecord *s;
        Node *parent;
        Node *leaf = NULL;
        while (true) {
            s = seek(deleteKey);
            if (!isCleanUp) {
                leaf = s->leaf;
                if (leaf->key != deleteKey) {
                    return;
                } else {
                    parent = s->parent;
                    if (deleteKey < parent->key) {
                        if (parent->lChild.cas(DPointer<Node, sizeof(size_t)>(leaf, 2), leaf)) {
                            isCleanUp = true;
                            //do cleanup
                            if (cleanUp(deleteKey, s)) {
                                return;
                            }
                        } else {
                            if (leaf == parent->lChild.ptr) {
                                cleanUp(deleteKey, s);
                            }
                        }
                    } else {
                        if (parent->rChild.cas(DPointer<Node, sizeof(size_t)>(leaf, 2), leaf)) {
                            isCleanUp = true;
                            //do cleanup
                            if (cleanUp(deleteKey, s)) {
                                return;
                            }
                        } else {
                            if (leaf == parent->rChild.ptr) {
                                //help other thread with cleanup
                                cleanUp(deleteKey, s);
                            }
                        }
                    }
                }
            } else {
                if (s->leaf == leaf) {
                    //do cleanup
                    if (cleanUp(deleteKey, s)) {
                        return;
                    }
                } else {
                    //someone helped with my cleanup. So I’m done
                    return;
                }
            }
        }
    }
    

    int setTag(int stamp) {
        switch (stamp) // set only tag
        {
            case 0:
                stamp = 1; // 00 to 01
                break;
            case 2:
                stamp = 3; // 10 to 11
                break;
        }
        return stamp;
    }
    int copyFlag(int stamp) {
        switch (stamp) //copy only the flag
        {
            case 1:
                stamp = 0; // 01 to 00
                break;
            case 3:
                stamp = 2; // 11 to 10
                break;
        }
        return stamp;
    }

    bool cleanUp(long key, SeekRecord *s) {
        Node *ancestor = s->ancestor;
        Node *parent = s->parent;
        Node *oldSuccessor;
        size_t oldStamp;
        Node *sibling;
        size_t siblingStamp;
        if (key < parent->key) { //xl case
            if (parent->lChild.mark > 1) { // check if parent to leaf edge is already flagged. 10 or 11
                //leaf node is flagged for deletion. tag the sibling edge to prevent any modification at this edge now
                sibling = parent->rChild.ptr;
                siblingStamp = parent->rChild.mark;
                siblingStamp = setTag(siblingStamp); // set only tag
                parent->rChild.cas(DPointer<Node, sizeof(size_t)>(sibling, siblingStamp), sibling);
                sibling = parent->rChild.ptr;
                siblingStamp = parent->rChild.mark;
            } else {
                //leaf node is not flagged. So sibling node must have been flagged for deletion
                sibling = parent->lChild.ptr;
                siblingStamp = parent->lChild.mark;
                siblingStamp = setTag(siblingStamp); // set only tag
                parent->lChild.cas(DPointer<Node, sizeof(size_t)>(sibling, siblingStamp), sibling);
                sibling = parent->lChild.ptr;
                siblingStamp = parent->lChild.mark;
            }
        } else { //xr case
            if (parent->rChild.mark > 1) { // check if parent to leaf edge is already flagged. 10 or 11
                //leaf node is flagged for deletion. tag the sibling edge to prevent any modification at this edge now
                sibling = parent->lChild.ptr;
                siblingStamp = parent->lChild.mark;
                siblingStamp = setTag(siblingStamp); // set only tag
                parent->lChild.cas(DPointer<Node, sizeof(size_t)>(sibling, siblingStamp), sibling);
                sibling = parent->lChild.ptr;
                siblingStamp = parent->lChild.mark;
            } else {
                //leaf node is not flagged. So sibling node must have been flagged for deletion
                sibling = parent->rChild.ptr;
                siblingStamp = parent->rChild.mark;
                siblingStamp = setTag(siblingStamp); // set only tag
                parent->rChild.cas(DPointer<Node, sizeof(size_t)>(sibling, siblingStamp), sibling);
                sibling = parent->rChild.ptr;
                siblingStamp = parent->rChild.mark;
            }
        }
        if (key < ancestor->key) {
            siblingStamp = copyFlag(siblingStamp); //copy only the flag
            oldSuccessor = ancestor->lChild.ptr;
            oldStamp = ancestor->lChild.mark;
            return (ancestor->lChild.cas(DPointer<Node, sizeof(size_t)>(sibling, siblingStamp), DPointer<Node, sizeof(size_t)>(oldSuccessor, oldStamp)));
        } else {
            siblingStamp = copyFlag(siblingStamp); //copy only the flag
            oldSuccessor = ancestor->rChild.ptr;
            oldStamp = ancestor->rChild.mark;
            return (ancestor->rChild.cas(DPointer<Node, sizeof(size_t)>(sibling, siblingStamp), DPointer<Node, sizeof(size_t)>(oldSuccessor, oldStamp)));
        }
    }
    
    SeekRecord* seek(long key) {
        DPointer<Node, sizeof(size_t)> parentField;
        DPointer<Node, sizeof(size_t)> currentField;
        Node *current;
        //initialize the seek record
        SeekRecord *s = new SeekRecord(grandParentHead, parentHead, parentHead, parentHead->lChild.ptr);
        parentField = s->ancestor->lChild;
        currentField = s->successor->lChild;
        while (currentField.ptr != NULL) {
            current = currentField.ptr;
            //move down the tree
            //check if the edge from the current parent node in the access path is tagged
            if (parentField.mark == 0 || parentField.mark == 2) { // 00, 10 untagged
                s->ancestor = s->parent;
                s->successor = s->leaf;
            }
            //advance parent and leaf pointers
            s->parent = s->leaf;
            s->leaf = current;
            parentField = currentField;
            if (key < current->key) {
                currentField = current->lChild;
            } else {
                currentField = current->rChild;
            }
        }
        return s;
    }
    void createHeadNodes() {
        long key = LONG_MAX;
        long value = LONG_MIN;
        parentHead = new Node(key, value, DPointer<Node, sizeof(size_t)>(new Node(key, value), 0), DPointer<Node, sizeof(size_t)>(new Node(key, value), 0));
        grandParentHead = new Node(key, value, DPointer<Node, sizeof(size_t)>(parentHead, 0), DPointer<Node, sizeof(size_t)>(new Node(key, value), 0));
    }
};
Node * LockFreeBST::grandParentHead = NULL;
Node * LockFreeBST::parentHead = NULL;
LockFreeBST *LockFreeBST::obj = NULL;



//llist *the_list;
LockFreeBST *the_bst;





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
    //unsigned long the_value;
    long k;
    int i;
    int last = -1;

    // before starting the test, we insert a number of elements in the data structure
    // we do this at each thread to avoid the situation where the entire data structure resides in the same memory node
    for (i = 0; i < d[j].num_add; ++i) {
        //the_value = (unsigned long) my_random(&seeds[0], &seeds[1], &seeds[2]) & rand_max;
        k = (long) my_random(&seeds[0], &seeds[1], &seeds[2]) & rand_max;
        // we make sure the insert was effective (as opposed to just updating an existing entry)
            the_bst->add(k);
    }

            //printf("line 118: reached here\n");
    // start the test
    while (op_cnt<operations) {
        // generate a value (node that rand_max is expected to be a power of 2)
        k = (long) my_random(&seeds[0], &seeds[1], &seeds[2]) & rand_max;
        // generate the operation
        op = my_random(&seeds[0], &seeds[1], &seeds[2]) & 0xff;
        if (op < read_thresh) {
            // do a find/read operation
            the_bst->lookup(k);
            d[j].num_search++;
            d[j].num_operations++;
            FAI_U32(&(op_cnt));
        } else if (last == -1) {
            // do a write i.e add operation
                the_bst->add(k);
                d[j].num_insert++;
                d[j].num_operations++;
                last = 1;
                FAI_U32(&(op_cnt));
        } else {
            // do a delete operation
                the_bst->remove(k);
                d[j].num_remove++;
                d[j].num_operations++;
                last = -1;
                FAI_U32(&(op_cnt));
        }
    }
    return;
}

int main(int argc, char *const argv[])
{
    //int numProcs = (int) sysconf(_SC_NPROCESSORS_ONLN);

    thread_data_t *data;

    //num_threads = numProcs - 1;
    //if ((num_threads%2)==1) {
    //    num_threads--;
    //}
    //num_threads = 1;
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

    the_bst = new LockFreeBST();

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
    //printf("Expected size: %ld Actual size: %d\n", reported_total,set_size(set));


    thread_shutdown();
    free(data);

    return 0;
}
