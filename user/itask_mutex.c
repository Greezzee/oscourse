#include <inc/lib.h>

jthread_mutex mutex;
jthread_mutex mutex_rec;

void func(void* arg) {
    (void)arg;
    cprintf("[%lx]: i am thread %016lx\n", THR_ENVX(thisenv->env_thr_cur), thisenv->env_thr_cur);
    jthread_mutex_lock(mutex);
    cprintf("[%lx]: starts\n", THR_ENVX(thisenv->env_thr_cur));
    for (int i = 0; i < 10; i++) {
        cprintf("[%lx]: working %d\n", THR_ENVX(thisenv->env_thr_cur), i);
        sys_yield();
    }
    cprintf("[%lx]: done\n\n", THR_ENVX(thisenv->env_thr_cur));
    jthread_mutex_unlock(mutex);
}

void func_rec(void* arg) {
    (void)arg;

    cprintf("[%lx]: i am thread %016lx\n", THR_ENVX(thisenv->env_thr_cur), thisenv->env_thr_cur);

    for (int i = 0; i < 5; i++) {
        jthread_mutex_lock(mutex_rec);
        cprintf("[%lx]: I locked mutex %d times\n", THR_ENVX(thisenv->env_thr_cur), i + 1);
        sys_yield();
    }
    for (int i = 0; i < 5; i++) {
        cprintf("[%lx]: I done my work %d times\n", THR_ENVX(thisenv->env_thr_cur), i + 1);
        sys_yield();
    }
    for (int i = 0; i < 5; i++) {
        cprintf("[%lx]: I unlocked mutex %d times\n", THR_ENVX(thisenv->env_thr_cur), i + 1);
        if (i == 4)
            cprintf("\n");
        jthread_mutex_unlock(mutex_rec);
        sys_yield();
    }
}

void
umain(int argc, char **argv) {

    mutex = jthread_mutex_init(NON_RECURSIVE_MUTEX);
    mutex_rec = jthread_mutex_init(RECURSIVE_MUTEX);

    jthread_mutex_lock(mutex);
    cprintf("Hello from inner part of mutex\n");
    sys_yield();
    cprintf("Exiting mutex...\n");
    jthread_mutex_unlock(mutex);

    thrid_t threads[5];

    cprintf("0: I start spawning\n");

    for (int i = 0; i < 5; i++) {
        jthread_create(&threads[i], func, NULL);
    }

    cprintf("0: I spawned 5 threads\n");
    
    for (int i = 0; i < 5; i++) {
        jthread_join(threads[i]);
    }

    cprintf("\n0: 5 more threads but recursive mutex!\n");

    for (int i = 0; i < 5; i++) {
        jthread_create(&threads[i], func_rec, NULL);
    }

    for (int i = 0; i < 5; i++) {
        jthread_join(threads[i]);
    }

    cprintf("0: I'm done!\n");

    jthread_mutex_destroy(mutex);
    jthread_mutex_destroy(mutex_rec);

    return;
}