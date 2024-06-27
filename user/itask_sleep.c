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

void
umain(int argc, char **argv) {
    cprintf("Starts at:\n");
    printNow();
    sys_sleep(1000);

    cprintf("1 s later:\n");
    printNow();
    sys_sleep(4000);

    cprintf("4s later:\n");
    printNow();

    for (int i = 0; i < 5; i++) {
        sys_sleep(2000);
        cprintf("2s later:\n");
        printNow();
    }
}