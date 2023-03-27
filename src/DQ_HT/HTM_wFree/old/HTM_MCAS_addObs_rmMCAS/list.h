#ifndef LLIST_H_
#define LLIST_H_

#include "atomic_ops_if.h"
#include "tm.h"
#include "spinlock.h"

#ifdef DEBUG
#define IO_FLUSH fflush(NULL)
#endif


extern libtle_spinlock_t global_rtm_mutex; 

typedef intptr_t val_t;

typedef struct node {
    val_t data;
    struct node *next;
    struct node *prev;
} node_t;

typedef struct llist {
    node_t *head;
    node_t *tail;
    uint32_t size;
} llist_t;

llist_t *list_new();

int list_contains(llist_t *the_list, val_t val, int tid);

int list_addback(llist_t *the_list, val_t val, int tid);
int list_addfront(llist_t *the_list, val_t val, int tid);

int list_removeback(llist_t *the_list, val_t val, int tid);
int list_removefront(llist_t *the_list, val_t val, int tid);
void list_delete(llist_t *the_list);
int list_size(llist_t *the_list);
void list_print(llist_t *the_list);

#endif
