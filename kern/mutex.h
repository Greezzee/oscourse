#ifndef JOS_KERN_MUTEX_H
#define JOS_KERN_MUTEX_H

#include <inc/thread.h>
#include <inc/env.h>
#include <kern/env.h>
#include <kern/thread.h>

extern struct Mutex *mutexes;

int mutexid2mutex(mutexid_t mutexid, struct Mutex** mutex);
void mutex_init();
int mutex_create(envid_t envid, struct Mutex** created_mutex);
int mutex_destroy(mutexid_t mutexid);
int mutex_lock(mutexid_t mutexid, thrid_t thr);
int mutex_unlock(mutexid_t mutexid);

#endif /* !JOS_KERN_MUTEX_H */