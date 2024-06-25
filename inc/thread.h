#ifndef JOS_INC_THREAD_H
#define JOS_INC_THREAD_H

#include <inc/types.h>
#include <inc/trap.h>
#include <inc/memlayout.h>

typedef int64_t thrid_t;
typedef int32_t envid_t;

#define LOG2NTHR    12
#define LOG2NTHR_PER_ENV 10
#define NTHR        (1 << LOG2NTHR)
#define THRX(thrid) ((thrid) & (NTHR - 1))
#define THR_ENVX(thrid) ((thrid) & (((1 << LOG2NTHR_PER_ENV) - 1) << LOG2NTHR))

struct Env;

enum {
    THR_FREE,
    THR_RUNNABLE,
    THR_RUNNING,
    THR_NOT_RUNNABLE
};

struct Thr {
    struct Trapframe thr_tf; /* Saved registers */
    struct Thr* thr_next;    /* Next working Thr (NULL if the last one) of this Env */
    struct Thr* thr_next_free; /* next global free thread */
    thrid_t thr_id;          /* Unique environment identifier */
    envid_t thr_env;   /* owner env id */

    unsigned thr_status;
    uint32_t thr_runs; /* Number of times thread has run */

    uintptr_t thr_stack_top;
};

#endif /* !JOS_INC_THREAD_H */