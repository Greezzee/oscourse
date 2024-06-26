#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <inc/thread.h>
#include <inc/env.h>
#include <kern/env.h>

extern struct Thr *thrs;
extern struct Thr *curthr;

extern struct Mutex *mutexes;

int thrid2thr(thrid_t thrid, struct Thr **thr_store);

void thr_init();
int thr_alloc(struct Thr **pthr, struct Env* env, size_t low_id);
void thr_free(struct Thr *thr);
int thr_create(envid_t envid, uint32_t force_thr_env_id, struct Thr** created_thr);
int thr_destroy(thrid_t thrid);
_Noreturn void thr_pop_tf(struct Trapframe *tf);
_Noreturn void thr_run(struct Thr *thr);
void thr_process_not_runnable(struct Thr *thr);
#if 0 // not implemented yet
int mutexid2mutex(mutexid_t mutexid, struct Mutex* mutex);
void mutex_init();
int mutex_create(envid_t envid, struct Mutex** created_mutex);
int mutex_destroy(mutexid_t mutexid);
int mutex_lock(mutexid_t mutexid);
int mutex_unlock(mutexid_t mutexid);
#endif
#endif /* !JOS_KERN_THREAD_H */