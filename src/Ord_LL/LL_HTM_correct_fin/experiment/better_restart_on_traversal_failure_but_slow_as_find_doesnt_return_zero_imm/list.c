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

int inc_it (node_t** tmp_it) {
    int tmp_flag = 0;
    //check for iterator being passed if NULL or not inside a TSX for strong isolation
    //the line "iterator = iterator->next if seg-faults inside TSX, it will be caught as a TSX_failure and TSX we be
    //re-attempted with the line after TM_BEGIN; hence, the if condition will set the flag on next attempt of "i" of TM_BEGIN
    //and we can re-start the traversal from head based on the flag_value in the main caller function
     TM_BEGIN()
        if ((*tmp_it)!=NULL) {
            (*tmp_it) = (*tmp_it)->next;
        } else {
            tmp_flag = 1;
        }
     TM_END() 
       return tmp_flag;     
    
}


int list_contains(llist_t *the_list, val_t val)
{
    node_t *it, *itn, *itp;
    it = the_list->head;
    itn = the_list->head;
    itp = the_list->head;
    //int flag_find = 0;
    while (it && it->next!=the_list->tail) {
            itn = it;
            if (inc_it(&itn)==1) {
                //iterator = the_list->head;
                if(itp) it = itp;
                else return 0;
            } else {
                if (itn && itn->data==val)
                    return 1;
                if (itn && itn->data>val)
                    return 0;
                if(!it || !itn) {
                    if(itp) it = itp;
                    else return 0;
                }
            }
            itp = it;
            it = itn;
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
    node_t *it, *itn, *itp;
    int flag_add = 0;    
    start_add:
            it = the_list->head;
            itn = the_list->head;
            itp = the_list->head;
            while(it && it->next!=the_list->tail){
                itn = it;
                if(inc_it(&itn)==1) {
                    if(itp) it = itp;
                    else goto start_add;
                } else {
                    if (itn && itn->data<val) {
                        itp = it;
                        it = itn;
                    } else if(itn && itn->data>=val) {
                        break;
                    } else if (itn==NULL || it==NULL) {
                        if(itp) it = itp;
                        else goto start_add;
                    }
                }
            }
            if (itn && itn->data==val) {
                return 0;
            }else if (it==NULL || itn==NULL) {
                goto start_add;

            } else {
                //here it is correct left node and itn is correct right node and itn->data > val
                //or
                //it and itn point to same node i.e last node in the list and itn->data < val 
                TM_BEGIN()
                    if ((it!=NULL) && (itn!=NULL)) {
                        if(it->data==itn->data && it->next==the_list->tail){
                            new_elem->next = it->next;
                            it->next = new_elem;
                            the_list->size = the_list->size + 1;
                        } else if (it->next==itn && it->data<val && itn->data>val) {
                            new_elem->next = it->next;
                            it->next = new_elem;
                            the_list->size = the_list->size + 1;
                        } else {
                            flag_add = 1;
                        }
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

}

/*
 * list_remove removes the node at the head of the stack; we can do free outside the TSX as 
 * the write to that node will abort any other on-going TSX and force them to re-start; 
 * right will be thread local and unique to this thread- so no worries about freeing the same pointer twice or more 
 */
int list_remove(llist_t *the_list, val_t val)
{
    node_t *it, *itn, *itp;
    int flag_remove = 0;
    start_remove:
            it = the_list->head;
            itn = the_list->head;
            itp = the_list->head;
            while(it && it->next!=the_list->tail){
                itn = it;
                if(inc_it(&itn)==1) {
                    if(itp) it = itp;
                    else goto start_remove;
                } else {
                    if (itn && itn->data<val) {
                        itp = it;    
                        it = itn;
                    } else if(itn && itn->data>=val) {
                        break;
                    } else if (itn==NULL || it==NULL) {
                        if(itp) it = itp;
                        else goto start_remove;
                    }
                }
            }

            if ((itn && itn->data!=val)) {
                return 0;
            }else if (it==NULL || (itn==NULL)) {
                goto start_remove;
            } else if (itn && itn->data==val) {
                //it and itn if they ever become equal, it will be only when it->data!=itn->data!=val and we reached end of the list; if itn->data==val and even if this is the last node in the list, we will exit from the while loop before it becomes == itn;

                        //we need to do free(right) inside a TSX because this will ensure that if any thread tries to add next to a node that is getting deleted, one of them will succeed and one will fail ensuring we never add to a node getting deleted and leave dangling nodes in the list; this is impossible to track using a single CAS without changing the algo itself and that's why both the none-TM/MCAS versions need another indirection- list_remove actually only marks something to be removed and list_search actually removes it: actually, since I write to right inside the transaction, freeing right inside is unnecessary and since we do strong isolation in READ during find, free can safely go out
                TM_BEGIN()
                    if (it!=NULL && it->next==itn && itn!=NULL && itn->data==val) {
                        it->next = itn->next;
                        itn->next = NULL;
                        the_list->size = the_list->size - 1;
                    } else {
                        itn = NULL;
                        flag_remove = 1;
                    }
                TM_END()
                if (flag_remove) {
                    flag_remove = 0;
                    goto start_remove;
                }
                free(itn);
                return 1;
            }

}
