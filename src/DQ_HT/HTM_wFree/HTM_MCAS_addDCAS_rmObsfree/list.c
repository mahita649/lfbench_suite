#include "list.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "spinlock.h"

int list_DCAS(node_t** adrs1, node_t** adrs2, node_t* old1, node_t* old2, node_t* new1, node_t* new2) {
   int fail = 1;
   TM_BEGIN()
       if(adrs1 && adrs2 && *adrs1 && *adrs2 && old1 && old2 && new1 && new2) {
           //check for both the pointers to be valid and not NULL; check for any node not becoming invalid inside the TSX
           if(*adrs1==old1 && *adrs2==old2){
                *adrs1 = new1;
                *adrs2 = new2;
                fail = 0;
            }
       }
    TM_END()
   
    return fail;
}

static node_t *new_node(val_t val, node_t *next, node_t *prev)
{
    // allocate node
    node_t *node = malloc(sizeof(node_t));
    node->data = val;
    node->next = next;
    node->prev = prev;
    return node;
}

llist_t *list_new()
{
    // allocate list
    llist_t *the_list = malloc(sizeof(llist_t));

    // now need to create the sentinel node
    the_list->head = new_node(INT_MIN, NULL, NULL);
    the_list->tail = new_node(INT_MAX, NULL, NULL);
    the_list->head->next = the_list->tail;
    the_list->tail->prev = the_list->head;
    the_list->size = 0;
    return the_list;
}

void list_delete(llist_t *the_list)
{
    // remove does this- so no need
}

int list_size(llist_t *the_list)
{
    return the_list->size;
}

/*
 * list_add inserts a new node with the given value val at the head; we can have multiple nodes in the stack with same value
 */
int list_addfront(llist_t *the_list, val_t val, int tid)
{
    node_t *right, *left;
    right = left = NULL;
    node_t *new_elem = new_node(val, NULL, NULL);
    while (1) {
            right = the_list->head->next;
            left = the_list->head;
            if(right)
                new_elem->next = right;
            else
                return 1;
            new_elem->prev = left;
            //MCAS begin
            if(right && (list_DCAS(&(left->next),&(right->prev),right,left,new_elem,new_elem) == 0)) {
                FAI_U32(&(the_list->size));
                return 1;
            }
    }
}



int list_addback(llist_t *the_list, val_t val, int tid)
{
    node_t *right, *left;
    right = left = NULL;
    node_t *new_elem = new_node(val, NULL, NULL);
    while (1) {
            left = the_list->tail->prev;
            right = the_list->tail;
            if(left)
                new_elem->prev = left;
            else
                return 1;

            new_elem->next = right;
            //MCAS begin
            if(left && (list_DCAS(&(left->next),&(right->prev),right,left,new_elem,new_elem) == 0)) {
                FAI_U32(&(the_list->size));
                return 1;
            }
    }
}


/*
 * list_remove removes the node at the head of the stack; we can do free outside the TSX as 
 * the write to that node will abort any other on-going TSX and force them to re-start; 
 * right will be thread local and unique to this thread- so no worries about freeing the same pointer twice or more 
 */
int list_removeback(llist_t *the_list, val_t val, int tid)
{
  //  printf("tid: %d : entered remove_back 182 with val %ld\n",tid,val);
    node_t *left;
    left = NULL;
    //if list is empty, no need to remove anything
    if (the_list->head->next==the_list->tail || the_list->tail->prev == the_list->head) {
        return 0;
    } 
            //MCAS begin
            TM_BEGIN()
                left = the_list->tail->prev;
                    if (left==the_list->head) {
                        left = NULL;
                    } else {
                        left->prev->next = the_list->tail;
                        the_list->tail->prev = left->prev;
                        the_list->size = the_list->size - 1;
                    }
            TM_END()
                    if (left!=NULL) {
                        free(left);
                        // printf("tid: %d : remove_back 178 with val %ld and returning 1\n",tid,val);
                        return 1;
                    } else {
                        return 0;
                    }
    //}
}


int list_removefront(llist_t *the_list, val_t val, int tid)
{
   // printf("tid: %d : entered remove_front 204 with val %ld and returning 1\n",tid,val);
    node_t *right;
    right = NULL;
    //if list is empty, no need to remove anything
    if (the_list->head->next==the_list->tail || the_list->tail->prev == the_list->head) {
        return 0;
    } 
            //MCAS begin
            TM_BEGIN()
                    right = the_list->head->next;
                    if (right==the_list->tail) {
                        right = NULL;
                    } else {
                        the_list->head->next = right->next;
                        right->next->prev = the_list->head;
                        the_list->size = the_list->size - 1;
                    }
            TM_END()
                    if (right!=NULL) {
                        free(right);
                        //   printf("tid: %d : remove_front 198 with val %ld and returning 1\n",tid,val);
                        return 1;
                    } else {
                        return 0;
                    }
}




void list_print(llist_t* the_list) {
    printf("printing all the elements in forward ordered\n");
    node_t* it = the_list->head;
    while(it) {
        printf("node is %ld\n",it->data);
            it = it->next;
            if (it==the_list->tail)
                break;
    }
    printf("printing all the elements in reverse ordered\n");
    node_t* itt = the_list->tail;
    while(itt) {
        printf("node is %ld\n",itt->data);
            itt = itt->prev;
            if (itt==the_list->head)
                break;
    }

   return; 
}
