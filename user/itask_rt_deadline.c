#include <inc/types.h>
#include <inc/time.h>
#include <inc/stdio.h>
#include <inc/lib.h>

bool
handler() {
    cprintf("[%08x]: I deadlined :c. But I will continue as regular!\n", thisenv->env_id);
    return true;
}

void printNow() {
    char time[20];
    int now = vsys_gettime();
    struct tm tnow;
    mktime(now, &tnow);
    snprint_datetime(time, 20, &tnow);
    cprintf("VDATE: %s\n", time);
}

void run_rt(int sleep) {
    add_deadline_handler(handler);
    if (sleep < 3)
        cprintf("I am env %08x, I'm running once every %d sec and working for %d sec\n", thisenv->env_id, 2 * sleep, sleep);
    else
        cprintf("I am env %08x, I'm running once every %d sec and working for %d sec, I will exeed deadline\n", thisenv->env_id, 2 * sleep, sleep);

    sys_env_change_class(thisenv->env_id, ENV_CLASS_REAL_TIME, sleep * 2000, 2900, sleep * 1000 + 100);
    for(int i = 0; i < 5; i++) {
        sys_sleep(sleep * 1000);
        printNow();
        if (thisenv->env_class == ENV_CLASS_REAL_TIME)
            cprintf("I am env %08x (once per %d sec), I'm rt, and I'm done with %d\n", thisenv->env_id, 2 * sleep, i);
        else
            cprintf("I am env %08x (once per %d sec), I'm regular one, and I'm done with %d\n", thisenv->env_id, 2 * sleep, i);       
        sys_periodic_wait();
    }
}

void
umain(int argc, char **argv) {
    
    for (int i = 0; i < 5; i++) {
        if (fork() == 0) {
            run_rt(i + 1);
            return;
        }
    }

    return;
}