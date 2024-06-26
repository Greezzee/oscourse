#include <inc/lib.h>

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