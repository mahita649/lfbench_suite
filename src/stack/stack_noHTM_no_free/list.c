#include "list.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


/*
 * list_contains returns 1 if a node with the value val is present in the stack; else returns 0;
 */
int list_contains(llist_t *the_list, val_t val)
{
    node_t *iterator = the_list->head;
    while((iterator!=the_list->tail) && (iterator->data!=val)){
        //the below line can cause seg-fault if memory reclamation is not done cleanly; for now we avoid this 
        //by having a memory-leak in remove function
        iterator = iterator->next;
    }

        if (iterator && (iterator->data == val)) {
            return 1;
        }
            return 0;
}

static node_t *new_node(val_t val, node_t *next)
{
    // allocate node
    node_t *node = (node_t*)malloc(sizeof(node_t));
    node->data = val;
    node->next = next;
    return node;
}

llist_t *list_new()
{
    // allocate list
    llist_t *the_list = (llist_t*)malloc(sizeof(llist_t));

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
    node_t* right;
    node_t *new_elem = new_node(val, NULL);
    while(1) {
        right = the_list->head->next;
        new_elem->next = right;
            if (CAS_PTR(&(the_list->head->next),right,new_elem)==right){
                FAI_U32(&(the_list->size));
                return 1;
            }
    }

}

/*
 * list_remove removes the node at the head of the stack; we don't free the node and currently allow memory-leak.
 */
int list_remove(llist_t *the_list, val_t val)
{
        node_t *right;
    while(1){
        if(the_list->head->next==the_list->tail)
            return 0; //stack is empty
        right = the_list->head->next;
        if (CAS_PTR(&(the_list->head->next), right, right->next) == right) {
            FAD_U32(&(the_list->size));
            //free(right);
            return 1;
        }
    }

}
