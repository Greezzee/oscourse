#include <inc/types.h>
#include <inc/time.h>
#include <inc/stdio.h>
#include <inc/lib.h>

void printNow() {
    char time[20];
    int now = vsys_gettime();
    struct tm tnow;
    mktime(now, &tnow);
    snprint_datetime(time, 20, &tnow);
    cprintf("VDATE: %s\n", time);
}

void run_rt(int sleep) {
    sys_env_change_class(thisenv->env_id, ENV_CLASS_REAL_TIME, sleep * 2000, sleep * 1000 + 200, sleep * 1000 + 100);
    cprintf("I am env %08x, I'm running once every %d sec and working for %d sec,\n", thisenv->env_id, 2 * sleep, sleep);
    for(int i = 0; i < 5; i++) {
        sys_sleep(sleep * 1000);
        printNow();
        cprintf("I am env %08x (once per %d sec), and I'm done with %d\n", thisenv->env_id, 2 * sleep, i);
        cprintf("--------------------------------\n");
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