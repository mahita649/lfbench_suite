#ifndef __PORTABLE_DEFNS_H__
#define __PORTABLE_DEFNS_H__

#define MAX_THREADS 256 /* Nobody will ever have more! */

#include "intel_defns.h"

#include <string.h>

#ifndef MB_NEAR_CAS
#define RMB_NEAR_CAS() RMB()
#define WMB_NEAR_CAS() WMB()
#define MB_NEAR_CAS()  MB()
#endif

typedef unsigned long int_addr_t;

typedef int bool_t;

#define FALSE 0
#define TRUE  1

#define ADD_TO(_v,_x)                                                   \
do {                                                                    \
    int __val = (_v), __newval;                                         \
    while ( (__newval = CASIO(&(_v),__val,__val+(_x))) != __val )       \
        __val = __newval;                                               \
} while ( 0 )

/*
 * Allow us to efficiently align and pad structures so that shared fields
 * don't cause contention on thread-local or read-only fields.
 */
#define CACHE_PAD(_n) char __pad ## _n [CACHE_LINE_SIZE]
#define ALIGNED_ALLOC(_s)                                       \
    ((void *)(((unsigned long)malloc((_s)+CACHE_LINE_SIZE*2) +  \
        CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1)))

/*
 * Interval counting
 */

typedef unsigned int interval_t;
#define get_interval(_i)                                                 \
do {                                                                     \
    interval_t _ni = interval;                                           \
    do { _i = _ni; } while ( (_ni = CASIO(&interval, _i, _i+1)) != _i ); \
} while ( 0 )

/*
 * POINTER MARKING
 */

#define get_marked_ref(_p)      ((void *)(((unsigned long)(_p)) | 1))
#define get_unmarked_ref(_p)    ((void *)(((unsigned long)(_p)) & ~1))
#define is_marked_ref(_p)       (((unsigned long)(_p)) & 1)


/*
 * SUPPORT FOR WEAK ORDERING OF MEMORY ACCESSES
 */

#ifdef WEAK_MEM_ORDER

#define MAYBE_GARBAGE (0)

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f)                                       \
do {                                                            \
    (_x) = (_f);                                                \
    if ( (_x) == MAYBE_GARBAGE ) { RMB(); (_x) = (_f); }        \
 while ( 0 )

#define WEAK_DEP_ORDER_RMB() RMB()
#define WEAK_DEP_ORDER_WMB() WMB()
#define WEAK_DEP_ORDER_MB()  MB()

#else

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f) ((_x) = (_f))

#define WEAK_DEP_ORDER_RMB() ((void)0)
#define WEAK_DEP_ORDER_WMB() ((void)0)
#define WEAK_DEP_ORDER_MB()  ((void)0)

#endif

/*
 * Strong LL/SC operations
 */



/*
 * MCS lock
 */

typedef struct qnode_t qnode_t;

struct qnode_t {
    qnode_t *next;
    int locked;
};

typedef struct {
    qnode_t *tail;
} mcs_lock_t;



/*
 * MCS fair MRSW lock.
 */

typedef struct mrsw_qnode_st mrsw_qnode_t;

struct mrsw_qnode_st {
#define CLS_RD 0
#define CLS_WR 1
    int class;
#define ST_NOSUCC   0
#define ST_RDSUCC   1
#define ST_WRSUCC   2
#define ST_SUCCMASK 3
#define ST_BLOCKED  4
    int state;
    mrsw_qnode_t *next;
};

typedef struct {
    mrsw_qnode_t *tail;
    mrsw_qnode_t *next_writer;
    int reader_count;
} mrsw_lock_t;


#define CLEAR_BLOCKED(_qn) ADD_TO((_qn)->state, -ST_BLOCKED)



#endif /* __PORTABLE_DEFNS_H__ */
