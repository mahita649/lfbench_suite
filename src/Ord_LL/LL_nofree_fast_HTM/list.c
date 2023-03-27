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
    node_t *iterator;
    iterator = the_list->head;
    //int flag_find = 0;
    while (iterator->next!=the_list->tail) {
            if (iterator->data==val)
                return 1;
            if (iterator->data>val)
                return 0;
            iterator = iterator->next; 
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
    node_t *it;
    int flag_add = 0;    
    start_add:
            it = the_list->head;
            while(it->next!=the_list->tail) {
                if (it->next->data==val)
                    return 0;
                if (it->next->data<val) {
                    it = it->next;
                } else if (it->next->data>val){
                    break;
                }
                    
            }

            //it is the correct left node where to insert even if it is before the tail as long as it is not NULL/deleted
                TM_BEGIN()
                    if ((it->data < val) && (it->next->data > val)) {
                        new_elem->next = it->next;
                        it->next = new_elem;
                        the_list->size = the_list->size + 1;
                    } else {
                        flag_add = 1;
                    }
                TM_END()
                    if (flag_add==1) {
                        flag_add = 0;
                        goto start_add;
                    }
                return 1;

}

/*
 * list_remove removes the node at the head of the stack; we can do free outside the TSX as 
 * the write to that node will abort any other on-going TSX and force them to re-start; 
 * right will be thread local and unique to this thread- so no worries about freeing the same pointer twice or more 
 */
int list_remove(llist_t *the_list, val_t val)
{
    node_t* right;
    node_t *it;
    int flag_remove = 0;
    start_remove:
        it = the_list->head->next;
            while(it->next!=the_list->tail){
                if (it->next->data<val) {
                        it = it->next; 
                } else if(it->next->data>=val) {
                    break;
                }
            }

            if (it->next==the_list->tail || it->next->data>val) {
                return 0;
            } else if (it->next->data==val) {
                TM_BEGIN()
                    if (it->next->data==val) {
                        right = it->next; //right can never be tail as it matches val
                        it->next = right->next;
                        right->next = the_list->head;
                        the_list->size = the_list->size - 1;
                    } else {
                        flag_remove = 1;
                    }
                TM_END()
                if (flag_remove) {
                    flag_remove = 0;
                    goto start_remove;
                }
                return 1;
            }

}
