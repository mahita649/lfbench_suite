#ifndef LLIST_H_
#define LLIST_H_

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

int list_contains(llist_t *the_list, val_t val);

int list_add(llist_t *the_list, val_t val);

int list_remove(llist_t *the_list, val_t val);
void list_delete(llist_t *the_list);
int list_size(llist_t *the_list);

#endif
