#include <inc/lib.h>

void thr1(void* arg) {
    (void)arg;
    cprintf("1: hello, world from thr1\n");
    cprintf("1: i am environment %08x, thread %016lx\n", thisenv->env_id, sys_getthrid());
}

void thr2(void* arg) {
    (void)arg;
    cprintf("2: hello, world from thr2\n");
    cprintf("2: i am environment %08x, thread %016lx\n", thisenv->env_id, sys_getthrid());
    for (int i = 0; i < 10; i++) {
        cprintf("2: Doing important work: %d\n", i);
        sys_yield();
    }
    cprintf("2: I'm done!\n");
    jthread_exit();
    cprintf("2: THIS SHOULD NOT BE WRITTEN\n");
}

void thr3(void* arg) {
    (void)arg;
    cprintf("3 hello, world from thr3\n");
    cprintf("3: i am environment %08x, thread %016lx. Im hoing to be infinite\n", thisenv->env_id, sys_getthrid());

    for(;;) {
        cprintf("3: INFINITE LOOP\n");
        sys_yield();
    }
}

void
umain(int argc, char **argv) {
    cprintf("0: hello, world\n");
    cprintf("0: i am environment %08x, thread %016lx\n", thisenv->env_id, sys_getthrid());

    thrid_t thr1_id = 0, thr2_id = 0, thr3_id;
    jthread_create(&thr1_id, thr1, NULL);
    cprintf("0: created thread1 %016lx\n", thr1_id);
    jthread_create(&thr2_id, thr2, NULL);
    cprintf("0: created thread2 %016lx\n", thr2_id);
    sys_yield();
    cprintf("0: I gave my brothers a time to work\n");
    cprintf("0: starting waiting for thread 2\n");
    //jthread_join(thr2_id);
    //cprintf("0: Thread 2 is done, going further\n");
    jthread_create(&thr3_id, thr3, NULL);
    cprintf("0: created thread3 %016lx\n", thr3_id);
    sys_yield();
    cprintf("0: Waiting...\n");
    for (int i = 0; i < 5; i++) {
        sys_yield();
        cprintf("0: Waiting %d\n", i);
    }
    cprintf("0: Long counter:\n");
    int sum = 0;
    for (int i = 0; i < 10000; i++) {
        for (int j = 0; j < i; j++)
            sum++;
        if (i % 1000 == 0)
            printf("0: sum is %d\n", sum);
    }
    cprintf("0: Cancelling 3\n");
    jthread_cancel(thr3_id);
    cprintf("0: Everybody done\n");
}