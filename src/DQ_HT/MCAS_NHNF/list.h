#ifndef LLIST_H_
#define LLIST_H_

#include "/home/mnagabh/gem5_20.1_with_TME_NVMM_integrated/latest/gem5/new_LL_mcas_TM/with_Fraser_mcas/concurrent-ll/include/atomic_ops_if.h"

#ifdef DEBUG
#define IO_FLUSH fflush(NULL)
#endif

typedef intptr_t val_t;

typedef struct node {
    val_t data;
    int* del;
    struct node *next;
    struct node *prev;
} node_t;

typedef struct llist {
    node_t *head;
    node_t *tail;
    node_t *dummy;
    uint32_t size;
} llist_t;

llist_t *list_new();

// return 0 if not found, positive number otherwise
int list_contains(llist_t *the_list, val_t val, int tid);

// return 0 if value already in the list, positive number otherwise
int list_addfront(llist_t *the_list, val_t val, int tid);
int list_addback(llist_t *the_list, val_t val, int tid);

// return 0 if value already in the list, positive number otherwise
int list_removeback(llist_t *the_list, val_t val, int tid);
int list_removefront(llist_t *the_list, val_t val, int tid);
void list_delete(llist_t *the_list);
int list_size(llist_t *the_list);

void list_print(llist_t *the_list);

#endif
