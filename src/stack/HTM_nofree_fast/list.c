#include "list.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "spinlock.h"


/*
 * list_contains returns 1 if a node with the value val is present in the stack; else returns 0; 
 * in this version, reading of nodes is not supported with strong isolation; 
 * this is slightly flawed but wanted to see if there are any seg-faults with this logic in gem5 at iterator - iterator->next;
 */
int list_contains(llist_t *the_list, val_t val)
{
    //printf("entered find\n");
    node_t *iterator;
    
        iterator = the_list->head->next;
        while((iterator->data!=val)){
            iterator = iterator->next;
            if (iterator==the_list->tail)
                break;
        }

            if ((iterator->data == val)) {
                return 1;
            }
            if (iterator==the_list->tail)
                return 0;

}

static node_t *new_node(val_t val, node_t *next)
{
    // allocate node
    node_t *node = malloc(sizeof(node_t));
    node->data = val;
    node->next = next;
    return node;
}

llist_t *list_new()
{
    // allocate list
    llist_t *the_list = malloc(sizeof(llist_t));

    // now need to create the sentinel node
    the_list->head = new_node(INT_MIN, NULL);
    the_list->tail = new_node(INT_MAX, NULL);
    the_list->head->next = the_list->tail;
    the_list->size = 0;
    return the_list;
}

void list_delete(llist_t *the_list)
{
    // TODO later: implement the deletion
}

int list_size(llist_t *the_list)
{
    return the_list->size;
}

/*
 * list_add inserts a new node with the given value val at the head; we can have multiple nodes in the stack with same value
 */
int list_add(llist_t *the_list, val_t val)
{
    node_t *new_elem = new_node(val, NULL);
        //printf("calling add\n");
        TM_BEGIN()
            new_elem->next = the_list->head->next;
            the_list->head->next = new_elem;
            the_list->size = the_list->size + 1;
        TM_END()
            return 1;
}

/*
 * list_remove removes the node at the head of the stack; we free the node inside a TSX so that 
 * a potential non-transactional read from list_contains will abort this; 
 * but even so, the logic wont be correct as if context switch happened with list_contains and list_remove executes 
 * completely removing a node list_contains was pointing to, when the thread doing list_contain, returns, it would fail
 */
int list_remove(llist_t *the_list, val_t val)
{
    int ret_value = 0;
    node_t* right;
        //printf("calling remove\n");
    TM_BEGIN()
        right = the_list->head->next;
        if (right==the_list->tail) {
            ret_value = 0;
        } else {
            the_list->head->next = right->next;
            the_list->size = the_list->size - 1;
            ret_value = 1;
        }
    TM_END()
    return ret_value;

}
