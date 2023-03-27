/* =============================================================================
 *
 * tm.h
 *
 * Utility defines for transactional memory.
 * Modified to support ARM TLE.
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Authors: Chi Cao Minh and Martin Trautmann
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 *
 * ------------------------------------------------------------------------
 *
 * For the license of ssca2, please see ssca2/COPYRIGHT
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 *
 * ------------------------------------------------------------------------
 *
 * Unless otherwise noted, the following license applies to STAMP files:
 *
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */

#  include <assert.h>
#  include "memory.h"
#  include "thread.h"
#  include "types.h"
#include <stdio.h>

#include <immintrin.h>
#include <xmmintrin.h>

#  define TM_ARG                        /* nothing */
#  define TM_ARG_ALONE                  /* nothing */
#  define TM_ARGDECL                    /* nothing */
#  define TM_ARGDECL_ALONE              /* nothing */
#  define TM_CALLABLE                   /* nothing */

#  define TM_STARTUP(numThread)         /* CPUID_RTM_CHECK; */ spinlock_init(&global_rtm_mutex);
#  define TM_SHUTDOWN()                 /* nothing */

#  define TM_THREAD_ENTER()             /* nothing */
#  define TM_THREAD_EXIT()              /* nothing */
#  define TM_BEGIN_WAIVER()
#  define TM_END_WAIVER()

#  define P_MALLOC(size)                malloc(size)  //memory_get(thread_getId(), size)
#  define P_FREE(ptr)                   free(ptr)    /* TODO: thread local free is non-trivial */
#  define TM_MALLOC(size)               malloc(size)  //memory_get(thread_getId(), size)
#  define TM_FREE(ptr)                  free(ptr)    /* TODO: thread local free is non-trivial */

/*mahita: is this right? libtle_spinlock_is_locked gives true on is_aq or not?*/
//#define TME_LOCK_IS_ACQUIRED    65535


                            /*printf("entered TM_BEGIN\n");  */
                                    /*__builtin_unreachable();\*/

#define TM_BEGIN() { \
                     unsigned int tries = 100, status ;    \
                        do { \
                            status = _xbegin(); \
                            if (status == _XBEGIN_STARTED) { \
                                if (!spinlock_isfree(&global_rtm_mutex)) { \
                                    _xabort(0xff); \
                                    __builtin_unreachable();\
                                } \
                                break; \
                            } \
                            tries--; \
                        } while ( tries>0); \
                        if((tries <= 0))  { \
                            spinlock_acquire(&global_rtm_mutex); }  
    

/*mahita comeback and see the above with fresh eyes and reimplement*/
#define TM_END()     if( tries>0 ) { \
                        _xend(); } \
                     else { \
                        spinlock_release(&global_rtm_mutex); } \
                   }



#    define TM_BEGIN_RO()                 TM_BEGIN()
#    define TM_RESTART()                  __tcancel(65535);
#    define TM_EARLY_RELEASE(var)         

#  define TM_SHARED_READ(var)         (var)
#  define TM_SHARED_WRITE(var, val)   ({var = val; var;})
#  define TM_LOCAL_WRITE(var, val)    ({var = val; var;})



/* =============================================================================
 *
 * End of tm.h
 *
 * =============================================================================
 */
