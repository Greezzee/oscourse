#ifndef JOS_INC_THREAD_H
#define JOS_INC_THREAD_H

#include <inc/types.h>
#include <inc/trap.h>
#include <inc/memlayout.h>

typedef int64_t thrid_t;
typedef int32_t envid_t;
typedef int32_t mutexid_t;

#define LOG2NTHR    12
#define LOG2NTHR_PER_ENV 8
#define NTHR        (1 << LOG2NTHR)
#define NTHR_PER_ENV (1 << LOG2NTHR_PER_ENV)
#define THRX(thrid) ((thrid) & (NTHR - 1))
#define THR_ENVX(thrid) (((thrid) & (((1 << LOG2NTHR_PER_ENV) - 1) << LOG2NTHR)) >> LOG2NTHR)
#define DEFAULT_PRIORITY 1

#define NMUTEX NTHR

struct Env;

enum {
    THR_FREE,
    THR_RUNNABLE,
    THR_RUNNING,
    THR_NOT_RUNNABLE
};

enum {
    THR_NOT_WAITING,
    THR_WAITING_JOIN,
    THR_WAITING_MUTEX,
    THR_WAITING_TIMER,
    THR_WAITING_IPC,
};

struct Thr {
    struct Trapframe thr_tf; /* Saved registers */
    struct Thr* thr_next;    /* Next working Thr (NULL if the last one) of this Env */
    struct Thr* thr_next_free; /* next global free thread */
    thrid_t thr_id;          /* Unique environment identifier */
    envid_t thr_env;   /* owner env id */

    unsigned thr_blocking_status;
    int64_t thr_block; /* id of thread we are join or id of mutex we are waiting to unlock */

    unsigned thr_status;
    uint32_t thr_runs; /* Number of times thread has run */

    uintptr_t thr_stack_top;

    uint32_t fixed_priority;
};

struct Mutex {
    bool mutex_is_locked;   /* is mutex in lock state */
    bool mutex_is_reserved; /* is mutex is given to some env */

    mutexid_t mutex_id; /* unique mutex id */
    envid_t mutex_owner_envid; /* env using this mutex */
    thrid_t mutex_owner_thrid; /* thread locking this mutex */

    struct Mutex* mutex_next; /* next free mutex in the mutex list */
};

#endif /* !JOS_INC_THREAD_H */