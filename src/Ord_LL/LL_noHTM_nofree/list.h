#ifndef LLIST_H_
#define LLIST_H_

//#include "atomic_ops_if.h"
#include "/home/mnagabh/gem5_20.1_with_TME_NVMM_integrated/latest/gem5/new_LL_mcas_TM/concurrent-ll/include/atomic_ops_if.h"

#ifdef DEBUG
#define IO_FLUSH fflush(NULL)
#endif

typedef intptr_t val_t;

typedef struct node {
    val_t data;
    struct node *next;
} node_t;

typedef struct llist {
    node_t *head;
    node_t *tail;
    uint32_t size;
} llist_t;

llist_t *list_new();

// return 0 if not found, return 1 otherwise
int list_contains(llist_t *the_list, val_t val);

// return 1 on successful insertion; return 0 if value is already in the list
int list_add(llist_t *the_list, val_t val);

// return 1 on successful removal; othrewise return a 0
int list_remove(llist_t *the_list, val_t val);
void list_delete(llist_t *the_list);
int list_size(llist_t *the_list);

node_t *list_search(llist_t *the_list, val_t val, node_t **left_node);

#endif
