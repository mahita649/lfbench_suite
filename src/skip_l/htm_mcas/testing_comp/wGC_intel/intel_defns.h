#ifndef __INTEL_DEFNS_H__
#define __INTEL_DEFNS_H__

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>

#ifndef INTEL
#define INTEL
#endif

#define CACHE_LINE_SIZE 64

#if 0
#define pthread_mutex_init(_m,_i) \
({ pthread_mutex_init(_m,_i); (_m)->__m_kind = PTHREAD_MUTEX_ADAPTIVE_NP; })
#endif


/*
 * I. Compare-and-swap.
 */

/*
 * This is a strong barrier! Reads cannot be delayed beyond a later store.
 * Reads cannot be hoisted beyond a LOCK prefix. Stores always in-order.
 */
#define CAS(_a, _o, _n)                                    \
({ __typeof__(_o) __o = _o; \
 __typeof__(_o) __oo = atomic_load_explicit(&__o,memory_order_seq_cst); atomic_compare_exchange_strong(_a, &(__oo), _n); \
   (__oo);                                                    \
})

#define CAS64 CAS

#define FAS(_a, _n)                                    \
({                                   \
     atomic_exchange(_a, _n); \
})


/* Update Integer location, return Old value. */
#define CASIO CAS
#define FASIO FAS
/* Update Pointer location, return Old value. */
#define CASPO CAS
#define FASPO FAS
/* Update 32/64-bit location, return Old value. */
#define CAS32O CAS
#define CAS64O CAS64

/*
 * II. Memory barriers. 
 *  WMB(): All preceding write operations must commit before any later writes.
 *  RMB(): All preceding read operations must commit before any later reads.
 *  MB():  All preceding memory accesses must commit before any later accesses.
 * 
 *  If the compiler does not observe these barriers (but any sane compiler
 *  will!), then VOLATILE should be defined as 'volatile'.
 */

#define MB()  __asm__ __volatile__ ("" : : : "memory")
#define WMB() __asm__ __volatile__ ("" : : : "memory")
#define RMB() MB()
#define VOLATILE /*volatile*/

/* On Intel, CAS is a strong barrier, but not a compile barrier. */
#define RMB_NEAR_CAS() WMB()
#define WMB_NEAR_CAS() WMB()
#define MB_NEAR_CAS()  WMB()


/*
 * III. Cycle counter access.
 */

typedef unsigned long long tick_t;
#define RDTICK() \
    ({ tick_t __t; __asm__ __volatile__ ("rdtsc" : "=A" (__t)); __t; })


/*
 * IV. Types.
 */

typedef unsigned char      _u8;
typedef unsigned short     _u16;
typedef unsigned int       _u32;
typedef unsigned long long _u64;

#endif /* __INTEL_DEFNS_H__ */
