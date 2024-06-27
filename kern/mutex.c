#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>
#include <inc/vsyscall.h>

#include <kern/mutex.h>
#include <kern/pmap.h>

struct Mutex *mutexes = NULL;
static struct Mutex *mutex_free_list;

int mutexid2mutex(mutexid_t mutexid, struct Mutex** mutex) {

    struct Mutex* buf;

    if (mutexid >= NMUTEX)
        return -E_BAD_MUTEX;

    buf = &mutexes[mutexid];
    if (!buf->mutex_is_reserved || buf->mutex_id != mutexid) {
        *mutex = NULL;
        return -E_BAD_MUTEX;
    }

    *mutex = buf;
    return 0;
}

void mutex_init() {
    assert(current_space);
    mutexes = kzalloc_region(NMUTEX * sizeof(*mutexes));
    memset(mutexes, 0, ROUNDUP(NMUTEX * sizeof(*mutexes), PAGE_SIZE));

    mutex_free_list = &mutexes[0];
	mutexes[0].mutex_id = 0;
    mutexes[0].mutex_is_reserved = false;
	for (int i = 1; i < NMUTEX; i++) {
		mutexes[i - 1].mutex_next =  &mutexes[i];
		mutexes[i].mutex_id = i;
	}
	mutexes[NTHR - 1].mutex_next = NULL;
}

int mutex_create(envid_t envid, struct Mutex** created_mutex) {
    struct Env* env;
    int res = envid2env(envid, &env, 0);
    if (res < 0)
        return -E_BAD_ENV;

    struct Mutex *mutex;
    if (!(mutex = mutex_free_list))
        return -E_NO_FREE_MUTEX;
    mutex_free_list = mutex->mutex_next;
    mutex->mutex_is_reserved = true;
    mutex->mutex_is_locked = false;
    mutex->mutex_owner_envid = envid;
    (*created_mutex) = mutex;
    return 0;
}

int mutex_destroy(mutexid_t mutexid) {
    struct Mutex* mutex;
    int res = mutexid2mutex(mutexid, &mutex);
    if (res < 0)
        return res;
    
    mutex->mutex_is_reserved = false;
    mutex->mutex_next = mutex_free_list;
    mutex_free_list = mutex;
    return 0;
}

int mutex_lock(mutexid_t mutexid, thrid_t thrid) {
    struct Mutex* mutex;
    int res = mutexid2mutex(mutexid, &mutex);
    if (res < 0)
        return res;
    
    if (mutex->mutex_is_locked && mutex->mutex_owner_thrid == thrid) {}
    else if (mutex->mutex_is_locked)
        return -E_MUTEX_LOCKED;
    else {
        mutex->mutex_is_locked = true;
        mutex->mutex_owner_thrid = thrid;
    }
    return 0;
}

int mutex_unlock(mutexid_t mutexid) {
    struct Mutex* mutex;
    int res = mutexid2mutex(mutexid, &mutex);
    if (res < 0)
        return res;

    if (!mutex->mutex_is_locked)
        return 0;
    
    mutex->mutex_is_locked = false;
    
    return 0;
}
