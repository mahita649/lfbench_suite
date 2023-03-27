#include "list.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "spinlock.h"

int inc_it (node_t** tmp_it, int dir) {
    int tmp_flag = 1;
    //check for iterator being passed if NULL or not inside a TSX for strong isolation
    //the line "iterator = iterator->next if seg-faults inside TSX, it will be caught as a TSX_failure and TSX we be
    //re-attempted with the line after TM_BEGIN; hence, the if condition will set the flag on next attempt of "i" of TM_BEGIN
    //and we can re-start the traversal from head based on the flag_value in the main caller function
     TM_BEGIN()
        if ((*tmp_it)!=NULL && ( (*tmp_it)->next != (*tmp_it) || (*tmp_it)->prev!=(*tmp_it)) ) {
            if (dir==1) {
                (*tmp_it) = (*tmp_it)->next;
            } else {
                (*tmp_it) = (*tmp_it)->prev;
            }
        } else {
            tmp_flag = 0;
        }
     TM_END() 
       return tmp_flag;     
    
}


int list_DCAS(node_t** adrs1, node_t** adrs2, node_t* old1, node_t* old2, node_t* new1, node_t* new2,int d,int idT,llist_t* tl) {
   int fail = 1;
   TM_BEGIN()
    if(d==-1 && (tl->head->next==tl->tail || tl->tail->prev==tl->head)) {
     //do nothing and exit   
    } else {
       if(adrs1 && adrs2 && *adrs1 && *adrs2 && old1 && old2 && new1 && new2) {
           //check for both the pointers to be valid and not NULL; check for any node not becoming invalid inside the TSX
         if((*adrs1)->next!=*adrs1 && (*adrs2)->next!=*adrs2 && old1->next!=old1 && old2->next!=old2 && new1->next!=new1 && new2->next!=new2 && (*adrs1)->prev!=*adrs1 && (*adrs2)->prev!=*adrs2 && old1->prev!=old1 && old2->prev!=old2 && new1->prev!=new1 && new2->prev!=new2) {
           if(*adrs1==old1 && *adrs2==old2){
                *adrs1 = new1;
                *adrs2 = new2;
                fail = 0;
                if(d==-1 && old1==old2){
                    old1->next = old1;
                    old1->prev = old1;
                }
            }
         }
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
    //printf("tid: %d : entered add_front 124 with val %ld\n",tid,val);
    node_t *right;
    right = NULL;
    node_t *new_elem = new_node(val, NULL, NULL);
    //while (1) {
           // printf("tid: %d : entered add_front_while with val %ld\n",tid,val);
            //MCAS begin
            TM_BEGIN()
                    right = the_list->head->next;
                    new_elem->next = right;
                    new_elem->prev = the_list->head;
                    the_list->head->next = new_elem;
                    right->prev = new_elem;
                    the_list->size = the_list->size + 1;
            TM_END()
                   // printf("tid: %d : add_front 128 with val %ld and returning 1\n",tid,val);
                    return 1;
     //       }
}



int list_addback(llist_t *the_list, val_t val, int tid)
{
   // printf("tid: %d : entered add_back 150 with val %ld\n",tid,val);
    node_t *left;
    left = NULL;
    node_t *new_elem = new_node(val, NULL, NULL);
    //while (1) {
           // printf("tid: %d : entered add_back_while with val %ld\n",tid,val);
            //MCAS begin
            TM_BEGIN()
                    left = the_list->tail->prev;
                    new_elem->next = the_list->tail;
                    new_elem->prev = left;
                    left->next = new_elem;
                    the_list->tail->prev = new_elem;
                    the_list->size = the_list->size + 1;
            TM_END()
                  //  printf("tid: %d : add_back 151 with val %ld and returning 1\n",tid,val);
                    return 1;
    //}
}






/*
 * list_remove removes the node at the head of the stack; we can do free outside the TSX as 
 * the write to that node will abort any other on-going TSX and force them to re-start; 
 * right will be thread local and unique to this thread- so no worries about freeing the same pointer twice or more 
 */
int list_removeback(llist_t *the_list, val_t val,int tid)
{
    node_t *left,*right, *left_prev;
    left = right = left_prev = NULL;
    while (1) {
            if ( (the_list->head->next == the_list->tail) || (the_list->tail->prev == the_list->head)) {
                return 0;
            }
            left = the_list->tail->prev;
            right = the_list->tail;
            if(left==the_list->head) {
                return 0;
            }
            left_prev = left;
            if (!inc_it(&left_prev,-1))
                return 0;
            //MCAS begin
            if (left_prev && (list_DCAS(&(left_prev->next),&(right->prev),left,left,right,left_prev,-1,tid,the_list) == 0)) {
                free(left);
                FAD_U32(&(the_list->size));
                return 1;
            }
    }
}


int list_removefront(llist_t *the_list, val_t val, int tid)
{
    node_t *right, *left, *right_next;
    right = left = right_next = NULL;
    while (1) {
            if ( (the_list->head->next == the_list->tail) || (the_list->tail->prev == the_list->head)) {
                return 0;
            }
            right = the_list->head->next;
            left = the_list->head;
            if (right==the_list->tail) {
                return 0;
            }
            right_next = right;
            if (!inc_it(&right_next,1))
                return 0;

            //MCAS begin
            if(right_next && (list_DCAS(&(left->next),&(right_next->prev),right,right,right_next,left,-1,tid,the_list) == 0)) {
                free(right);
                FAD_U32(&(the_list->size));
                return 1;
            }
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
