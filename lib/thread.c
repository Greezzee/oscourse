#include <inc/lib.h>
#define MUTEX_PER_ENV 256

int jthread_create(thrid_t* thr, void(*start_routine)(void*), void* arg) {
    thrid_t thr_id = sys_thr_create();
    if (thr_id < 0)
        return -E_BAD_THR;
    if (thr_id != CURTHRID) {
        if (thr)
            *thr = thr_id;
    }
    else {
        start_routine(arg);
        jthread_exit();
    }
    return 0;
}

int jthread_exit() {
    if (thisenv->env_thr_count == 1)
        close_all();
    return sys_thr_exit();
}

int jthread_cancel(thrid_t thr_id) {
    return sys_thr_cancel(thr_id);
}

int jthread_join(thrid_t thr_id) {
    return sys_thr_join(thr_id);
}

struct jthread_mutex_data {
    mutexid_t global_mutex_id;
    bool is_init;
    bool is_recursive;
    bool is_locked;
    size_t lock_depth;

    thrid_t owner_thread;
};

struct jthread_mutex_data mutex_data[MUTEX_PER_ENV];

jthread_mutex jthread_mutex_init(int param) {
    jthread_mutex i = 0;
    for (; i < MUTEX_PER_ENV; i++)
        if (!mutex_data[i].is_init)
            break;
    if (i == MUTEX_PER_ENV)
        return -E_NO_FREE_MUTEX;
    
    mutex_data[i].global_mutex_id = sys_mutex_create();
    if (mutex_data[i].global_mutex_id < 0)
        return mutex_data[i].global_mutex_id;
    
    mutex_data[i].is_init = true;
    mutex_data[i].is_recursive = (param == RECURSIVE_MUTEX);
    mutex_data[i].lock_depth = 0;
    mutex_data[i].is_locked = false;
    return i;
}

int jthread_mutex_destroy(jthread_mutex mutex) {
    if (mutex >= MUTEX_PER_ENV || !mutex_data[mutex].is_init)
        return -E_BAD_MUTEX;
    
    int res = sys_mutex_destroy(mutex_data[mutex].global_mutex_id);
    if (res < 0)
        return res;

    mutex_data[mutex].is_init = false;
    return 0;
}

int jthread_mutex_lock(jthread_mutex mutex) {
    if (mutex >= MUTEX_PER_ENV || !mutex_data[mutex].is_init)
        return -E_BAD_MUTEX;

    thrid_t this_thr = thisenv->env_thr_cur;
    struct jthread_mutex_data* this_mutex = mutex_data + mutex;

    if (!this_mutex->is_locked) {
        this_mutex->is_locked = true;
        this_mutex->owner_thread = this_thr;
        this_mutex->lock_depth = 1;
        return 0;
    }

    if (this_mutex->owner_thread == this_thr && this_mutex->is_recursive) {
        this_mutex->lock_depth++;
        return 0;
    }

    int res = sys_mutex_block_thr(this_mutex->global_mutex_id, this_mutex->owner_thread);
    if (res < 0)
        return res;
    return 0;
}

int jthread_mutex_unlock(jthread_mutex mutex) {
    if (mutex >= MUTEX_PER_ENV || !mutex_data[mutex].is_init)
        return -E_BAD_MUTEX;

    struct jthread_mutex_data* this_mutex = mutex_data + mutex;

    if (!this_mutex->is_locked)
        return 0;
    
    this_mutex->lock_depth--;
    if (this_mutex->lock_depth == 0)
        return sys_mutex_unlock(this_mutex->global_mutex_id);

    return 0;
}