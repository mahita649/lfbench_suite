#include "list.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "spinlock.h"


/*
 * list_contains returns 1 if a node with the value val is present in the stack; else returns 0; stronger isolation here  
 */
int list_contains(llist_t *the_list, val_t val)
{
    //printf("entered find\n");
    node_t *iterator;
    iterator = the_list->head;
    int flag_find = 0;
    while (iterator && iterator->next!=the_list->tail) {
            if (iterator && iterator->data==val)
                return 1;
            //check for iterator is NULL or not inside a TSX for strong isolation
            //the line "iterator = iterator->next if seg-faults inside TSX, it will be caught as a TSX_failure and TSX we be
            //re-attempted with the line after TM_BEGIN; hence, the if condition will set flag_find on next attempt; i
            //and we can re-start the traversal from head. 
            TM_BEGIN()
                if (iterator!=NULL) {
                    iterator = iterator->next;
                } else {
                    flag_find = 1;
                }
            TM_END() 
            //printf("tm_end called\n");
                    
                    if (flag_find) {
                       //printf("called tcancel\n");
                       //iterator = the_list->head;
                       // flag_find = 0;
                       return 0;
                    }
    }
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
 * list_remove removes the node at the head of the stack; we can do free outside the TSX as 
 * the write to that node will abort any other on-going TSX and force them to re-start; 
 * right will be thread local and unique to this thread- so no worries about freeing the same pointer twice or more 
 */
int list_remove(llist_t *the_list, val_t val)
{
    int ret_value = 1;
    node_t* right;
        //printf("calling remove\n");
    TM_BEGIN()
        right = the_list->head->next;
        if(right == the_list->tail) {
            right = NULL;
            ret_value = 0;
        } else {
            the_list->head->next = right->next;
            the_list->size = the_list->size - 1;
        }
    TM_END()
        free(right);
    return ret_value;

}
