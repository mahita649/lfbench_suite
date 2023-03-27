#include "list.h"
#include "mcas.c"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


static node_t *new_node(val_t val, node_t *next, node_t* prev)
{
    // allocate node
    node_t *node = (node_t*)malloc(sizeof(node_t));
    node->data = val;
    node->next = next;
    node->prev = prev;
    return node;
}

llist_t *list_new()
{
    // allocate list
    llist_t *the_list = (llist_t*)malloc(sizeof(llist_t));

    // now need to create the sentinel node
    the_list->head = new_node(INT_MIN, NULL, NULL);
    the_list->tail = new_node(INT_MAX, NULL, NULL);
    the_list->dummy = new_node(INT_MAX-5, NULL, NULL);
    the_list->head->next = the_list->tail;
    the_list->tail->prev = the_list->head;
    the_list->size = 0;

    static int mcas_inited = 0;
    if ( !CASIO(&mcas_inited, 0, 1) ) 
        mcas_init();

    return the_list;
}

void list_delete(llist_t *the_list)
{
    // FIXME: implement the deletion
}

int list_size(llist_t *the_list)
{
    return the_list->size;
}

/*
 * list_add inserts a new node with the given value val in the list
 * (if the value was absent) or does nothing (if the value is already present).
 */
int list_addfront(llist_t *the_list, val_t val, int tid) {
    //printf("entered addfront tid %d\n",tid);
    node_t *new_elem = new_node(val, the_list->dummy, the_list->dummy);
    node_t* right;
    int len = 4;
    while(1) {
        right = the_list->head->next;
        if ( (get_markedness(right)==0) && (get_markedness(right->prev)==0) ) {
                 if( (right->prev!=right) && mcas(len, (void**)&(new_elem->next),the_list->dummy,right,(void**)&(the_list->head->next),right,new_elem,(void**)&(right->prev),the_list->head,new_elem,(void**)&(new_elem->prev),the_list->dummy,the_list->head)) {
                        FAI_U32(&(the_list->size));
                        return 1;
                } else {
                    return 0;
                }
        } else {
            return 0;
        }
    }

}




int list_addback(llist_t *the_list, val_t val, int tid)
{
    //printf("entered addback tid %d\n",tid);
    node_t *new_elem = new_node(val, the_list->dummy,the_list->dummy);
    node_t* left;
    int len = 4;
    while(1) {
        left = the_list->tail->prev;
        if ( (get_markedness(left)==0) && (get_markedness(left->next)==0) ) {
                 if( (left->next!=left) && mcas(len, (void**)&(new_elem->next),the_list->dummy,the_list->tail,(void**)&(left->next),the_list->tail,new_elem,(void**)&(new_elem->prev),the_list->dummy,left,(void**)&(the_list->tail->prev),left,new_elem)) {
                        FAI_U32(&(the_list->size));
                        return 1;
                } else {
                    return 0;
                }
        } else {
            return 0;
        }
    }

}



/*
 * list_remove deletes a node with the given value val (if the value is present)
 * or does nothing (if the value is already present).
 * The deletion is logical and consists of setting the node mark bit to 1.
 */
int list_removeback(llist_t *the_list, val_t val, int tid)
{

    //printf("entered remove tid %d\n",tid);
    node_t *left_prev, *left;
    int len = 4;

    while (1) {
        //the list is empty case
        if ((the_list->tail->prev == the_list->head) || (the_list->head->next == the_list->tail))
            return 0;
        

        left = the_list->tail->prev;
        if(left==the_list->head)
            return 0;

        if ( (get_markedness(left)==0) && left->prev!=left) {
            left_prev = left->prev;
        } else {
            return 0;
        }

      if(left_prev!=NULL) {
        if ((get_markedness(left)==0) && (get_markedness(left_prev)==0) && (get_markedness(left_prev->next)==0)) {
                 if ((left->next!=left) && mcas(len,(void**)&(left->next),the_list->tail,left,(void**)&(left_prev->next),left,the_list->tail,(void**)&(left->prev),left_prev,left,(void**)&(the_list->tail->prev),left,left_prev )) {
                        FAD_U32(&(the_list->size));
                        //free(left);
                        return 1;
                } else {
                    return 0;
                }
        } else {
            return 0;
        }
       } else {
            return 0;
       }
    }
}


int list_removefront(llist_t *the_list, val_t val, int tid) {

    //printf("entered removefront tid %d\n",tid);
    node_t *right, *right_next;
    int len = 4;


  while (1) {
        //the list is empty case
        if ((the_list->head->next == the_list->tail) || (the_list->tail->prev == the_list->head))
            return 0;

        right = the_list->head->next;
        if (right==the_list->tail)
            return 0;

        if ( (get_markedness(right)==0) && right->next!=right) {
            right_next = right->next;
        } else {
            return 0;
        }

        if (right_next!=NULL) {
        if ((get_markedness(right)==0) && (get_markedness(right_next)==0) && (get_markedness(right_next->prev)==0)) {
                 if ((right->prev!=right) && mcas(len,(void**)&(the_list->head->next),right,right_next,(void**)&(right->next),right_next,right,(void**)&(right->prev),the_list->head,right,(void**)&(right_next->prev),right,the_list->head)) {
                        FAD_U32(&(the_list->size));
                        //free(left);
                        return 1;
                } else {
                    return 0;
                }
        } else {
            return 0;
        }
    } else {
        return 0;
    }
  }
}


void list_print(llist_t *the_list) {
    printf("printing all the elements in forward ordered\n");
    node_t *it = the_list->head;
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
