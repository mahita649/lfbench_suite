/******************************************************************************
 * skip_mcas.c
 * 
 * Skip lists, allowing concurrent update by use of MCAS primitive.
 * 
 * Copyright (c) 2001-2003, K A Fraser
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define __SET_IMPLEMENTATION__

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "set.h"
#include <time.h>
#include "spinlock.h"
#include <stdio.h>

/*
 * SKIP LIST
 */
typedef struct node_st node_t;
typedef struct set_st set_t;
typedef node_t *sh_node_pt;

struct node_st
{
    int        level;
    setkey_t  k;
    setval_t  v;
    sh_node_pt next[1];
};

struct set_st
{
    node_t head;
};


/*
 * PRIVATE FUNCTIONS
 */

int inc_it (node_t** tmp_it, int j, int tid) {
    int tmp_flag = 1;
    int fid = 0;
     TM_BEGIN(fid,tid)
        if ( (*tmp_it) && (*tmp_it)->next[j] ) {
                (*tmp_it) = (*tmp_it)->next[j];
        } else {
            tmp_flag = 0;
        }
     TM_END() 
       return tmp_flag;     
    
}

/*
 * Random level generator. Drop-off rate is 0.5 per level.
 * Returns value 1 <= level <= NUM_LEVELS.
 */
static int get_level()
{
    unsigned long r = rand();
    int l = 1;
    r = (r >> 4) & ((1 << (NUM_LEVELS-1)) - 1);
    while ( (r & 1) ) { l++; r >>= 1; }
    return(l);
}


/*
 * Allocate a new node, and initialise its @level field.
 * NB. Initialisation will eventually be pushed into garbage collector,
 * because of dependent read reordering.
 */
static node_t *alloc_node()
{
    int l;
    node_t *n;
    l = get_level();
    n = (node_t*) malloc(sizeof(*n) + (l-1)*sizeof(node_t *));
    memset(n, 0, sizeof(*n) + (l-1)*sizeof(node_t *));
    n->level = l;
    return(n);
}

void free_node(node_t* n) {
    for (int i=0;i<n->level;i++) {
        free(&(n->next[i]));
    }
    free(n);
    return;
}

/*
 * Search for first non-deleted node, N, with key >= @k at each level in @l.
 * RETURN VALUES:
 *  Array @pa: @pa[i] is non-deleted predecessor of N at level i
 *  Array @na: @na[i] is N itself, which should be pointed at by @pa[i]
 *  MAIN RETURN VALUE: same as @na[0].
 */
static sh_node_pt search_predecessors(
    set_t *l, setkey_t k, sh_node_pt *pa, sh_node_pt *na, int tid)
{
    sh_node_pt x, x_next;
    setkey_t  x_next_k;
    int        i;

restart_search:
    x = &l->head;
    for ( i = NUM_LEVELS - 1; i >= 0; i-- )
    {
        for ( ; ; )
        {
            x_next = x;

            if(!(inc_it(&x_next,i,tid))) {
                goto restart_search;
            } else {
                if ( x_next && x_next->k >= k ) break;
                x = x_next;
            }
        }

        if(x && x_next)  {
            if ( pa ) pa[i] = x;
            if ( na ) na[i] = x_next;
        } else {
            goto restart_search;
        }
    }

    return(x_next);
}

int finish_delete(sh_node_pt x, sh_node_pt *preds,setkey_t k, int tid)
{
    int level, i, ret = 0;
    sh_node_pt x_next;
    setkey_t x_next_k;
    int fid = 1;

    TM_BEGIN(fid,tid)
       if (x && x->k==k && x->v) {
                ret = 1;
                for (int i =0; i<x->level;i++) {
                    if( !(preds[i] && preds[i]->next[i]==x) ) {
                        //fail
                        ret = 0;
                        break;
                    }
                }

                if(ret==1) {
                    x->v = NULL;
                    for (int i=0; i<x->level;i++) {
                            preds[i]->next[i] = x->next[i];
                            x->next[i] = NULL;
                    }
                }
       }
    TM_END()

    if(ret) {
        //free_node(x);
        free(x);
    }
        
    return ret;
}


/*
 * PUBLIC FUNCTIONS
 */

set_t *set_alloc(void)
{
    set_t *l;
    node_t *n;
    int i;

    n = malloc(sizeof(*n) + (NUM_LEVELS-1)*sizeof(node_t *));
    memset(n, 0, sizeof(*n) + (NUM_LEVELS-1)*sizeof(node_t *));
    n->k = SENTINEL_KEYMAX;

    /*
     * Set the forward pointers of final node to other than NULL,
     * otherwise READ_FIELD() will continually execute costly barriers.
     * Note use of 0xfc -- that doesn't look like a marked value!
     */
    memset(n->next, 0xfc, NUM_LEVELS*sizeof(node_t *));

    l = malloc(sizeof(*l) + (NUM_LEVELS-1)*sizeof(node_t *));
    l->head.k = SENTINEL_KEYMIN;
    l->head.level = NUM_LEVELS;
    for ( i = 0; i < NUM_LEVELS; i++ )
    {
        l->head.next[i] = n;
    }
    return(l);
}


int set_update(set_t *l, setkey_t k, setval_t v, int overwrite, int tid)
{
    int rVa = 0;
    setval_t  ov, new_ov;
    sh_node_pt preds[NUM_LEVELS], succs[NUM_LEVELS];
    sh_node_pt succ, new = NULL;
    int        i, ret=0;

    int fid = 2;

    k = CALLER_TO_INTERNAL_KEY(k);


    do {
    retry:
        ov = NULL;

        succ = search_predecessors(l, k, preds, succs, tid);
    
        if ( succ && succ->k == k )
        {
            /* Already a @k node in the list: update its mapping. */
            goto out;
        }

        if(!succ) {
            goto out;
        }


        /* Not in the list, so initialise a new node for insertion. */
        if ( new == NULL )
        {
            new    = alloc_node();
            new->k = k;
            new->v = v;
        }

        TM_BEGIN(fid,tid)

            ret = 1;
            for ( i = 0; i < new->level; i++ ) {
                if( !(succs[i] && preds[i] && (preds[i]->next[i]==succs[i]) && succs[i]->k>k && preds[i]->k<k) ) {
                    ret = 0;
                    break;
                }
            }
            if(ret==1) {
                for ( i = 0; i < new->level; i++ ) {
                        new->next[i] = succs[i];
                        preds[i]->next[i] = new;
                }
            }
        
        TM_END()

    }
    while ( !ret );
        rVa = 1;

 out:
    return rVa;
}


int set_remove(set_t *l, setkey_t k, int tid)
{
   int rV = 0;
    setval_t  v = NULL;
    sh_node_pt preds[NUM_LEVELS], x;

    k = CALLER_TO_INTERNAL_KEY(k);
    
    do {
        x = search_predecessors(l, k, preds, NULL, tid);
        if ( x && (x->k > k) ) goto out;
    } while (!finish_delete(x, preds,k,tid));
    rV = 1;
 
out:
    return rV;
}


int set_lookup(set_t *l, setkey_t k, int tid) {
    int ret = 0;
    sh_node_pt x;

    k = CALLER_TO_INTERNAL_KEY(k);

    x = search_predecessors(l, k, NULL, NULL,tid);
    if ( x && x->k == k ) {
        ret = 1;
    }

    return(ret);
}


int set_size(set_t* s) {
    int size = 0;
    sh_node_pt x;
    x = &s->head;
    while(1) {
        size++;
        x = x->next[0];
        //printf("size and x->k here is %d and %d\n",size, x->k);
        if (x->next[0]->k==SENTINEL_KEYMAX)
            break;
    }
    return size;
}
