#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <inc/thread.h>
#include <inc/env.h>
#include <kern/env.h>

extern struct Thr *thrs;
extern struct Thr *curthr;

int thrid2thr(thrid_t thrid, struct Thr **thr_store);

void thr_init();
int thr_alloc(struct Thr **pthr, struct Env* env, size_t low_id);
void thr_free(struct Thr *thr);
int thr_create(envid_t envid);
int thr_destroy(thrid_t thrid);
_Noreturn void thr_pop_tf(struct Trapframe *tf);
_Noreturn void thr_run(struct Thr *thr);
#endif /* !JOS_KERN_THREAD_H */