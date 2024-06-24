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

void recv_after_send() {
    envid_t who;
    printNow();
    if ((who = fork()) != 0) {
        int to_send = 42;
        int res = ipc_send_timed(who, to_send, NULL, 0, 0, 2000);
        if (res)
            cprintf("Error in ipc_send_timed: %i\n", res);
        sys_env_destroy(0);
    }
    sys_sleep(1000);
    
    int res = ipc_recv_timed(&who, 0, 0, 0, 1000);
    if (!who) {
        cprintf("Error in ipc_recv_timed: %i\n", res);
        return;
    }
    cprintf("%x got %d from %x\n", sys_getenvid(), res, who);
    printNow();
}

void send_after_recv() {
    envid_t who;
    printNow();
    if ((who = fork()) != 0) {
        int to_send = 42;
        sys_sleep(1000);
        int res = ipc_send_timed(who, to_send, NULL, 0, 0, 1000);
        if (res)
            cprintf("Error in ipc_send_timed: %i\n", res);
        sys_env_destroy(0);
    }
    int res = ipc_recv_timed(&who, 0, 0, 0, 2000);
    if (!who) {
        cprintf("Error in ipc_recv_timed: %i\n", res);
        return;
    }
    cprintf("%x got %d from %x\n", sys_getenvid(), res, who);
    printNow();
}

void
umain(int argc, char **argv) {
    cprintf("Test recv after send:\n");
    recv_after_send();
    cprintf("\nTest send after recv:\n");
    send_after_recv();
}