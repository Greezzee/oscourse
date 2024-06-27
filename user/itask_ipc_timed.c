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

void test_ipc_timed(uint64_t send_timeout, uint64_t recv_timeout, uint64_t send_sleep, uint64_t recv_sleep) {
    envid_t who;
    printNow();
    if ((who = fork()) != 0) {
        int to_send = 42;
        sys_sleep(send_sleep);
        int res = ipc_send_timed(who, to_send, NULL, 0, 0, send_timeout);
        if (res)
            cprintf("Error in ipc_send_timed: %i\n", res);
        wait(who);
        return;
    }
    sys_sleep(recv_sleep);
    
    int res = ipc_recv_timed(&who, 0, 0, 0, recv_timeout);
    if (!who) {
        cprintf("Error in ipc_recv_timed: %i\n", res);
        sys_env_destroy(0);
    }
    cprintf("%x got %d from %x\n", sys_getenvid(), res, who);
    printNow();
    sys_env_destroy(0);
}

void test_ipc_send_to_timed(uint64_t recv_timeout, uint64_t send_sleep, uint64_t recv_sleep) {
    envid_t who;
    printNow();
    if ((who = fork()) != 0) {
        int to_send = 42;
        sys_sleep(send_sleep);
        ipc_send(who, to_send, NULL, 0, 0);
        wait(who);
        return;
    }
    sys_sleep(recv_sleep);
    
    int res = ipc_recv_timed(&who, 0, 0, 0, recv_timeout);
    if (!who) {
        cprintf("Error in ipc_recv_timed: %i\n", res);
        sys_env_destroy(0);
    }
    cprintf("%x got %d from %x\n", sys_getenvid(), res, who);
    printNow();
    sys_env_destroy(0);
}

void test_ipc_recv_from_timed(uint64_t send_timeout, uint64_t send_sleep, uint64_t recv_sleep) {
    envid_t who;
    printNow();
    if ((who = fork()) != 0) {
        int to_send = 42;
        sys_sleep(send_sleep);
        int res = ipc_send_timed(who, to_send, NULL, 0, 0, send_timeout);
        if (res < 0)
            cprintf("Error in ipc_send_timed: %i\n", res);
        wait(who);
        return;
    }
    sys_sleep(recv_sleep);
    
    int res = ipc_recv(&who, 0, 0, 0);
    if (!who) {
        cprintf("Error in ipc_recv_timed: %i\n", res);
        sys_env_destroy(0);
    }
    cprintf("%x got %d from %x\n", sys_getenvid(), res, who);
    printNow();
    sys_env_destroy(0);
}

void timed_pong(uint64_t send_timeout, uint64_t recv_timeout) {
    envid_t who;
    printNow();
    if ((who = fork()) != 0) {
        int to_send = 0;
        int i = 0;
        for (;; i++) {
            sys_sleep(i * 10);
            cprintf("[%d] %x sent %d to %x\n", i, sys_getenvid(), to_send, who);
            int res = ipc_send_timed(who, to_send, NULL, 0, 0, send_timeout);
            if (res < 0) {
                cprintf("[%d] %x: Error in ipc_send_timed: %i\n", i, sys_getenvid(), res);
                wait(who);
                return;
            }
            sys_sleep(i * 10);

            envid_t from_id;
            to_send = ipc_recv_timed(&from_id, 0, 0, 0, recv_timeout);
            if (!from_id || from_id != who) {
                cprintf("[%d] %x: Error in ipc_recv_timed: %i\n", i, sys_getenvid(), to_send);
                wait(who);
                return;
            }
            cprintf("[%d] %x got %d from %x\n", i, sys_getenvid(), to_send, from_id);
            to_send++;

            printNow();
        }
        wait(who);
        return;
    }
    int i = 0;
    for (;; i++) {
        int res = ipc_recv_timed(&who, 0, 0, 0, recv_timeout);
        if (!who) {
            cprintf("[%d] %x: Error in ipc_recv_timed: %i\n", i, sys_getenvid(), res);
            sys_env_destroy(0);
        }
        cprintf("[%d] %x got %d from %x\n", i, sys_getenvid(), res, who);

        cprintf("%x sent %d to %x\n", sys_getenvid(), res, who);
        res++;
        res = ipc_send_timed(who, res, NULL, 0, 0, send_timeout);
        if (res < 0) {
            cprintf("[%d] %x: Error in ipc_send_timed: %i\n", i, sys_getenvid(), res);
            sys_env_destroy(0);
        }
    }
}

const char *str1 = "hello child environment! how are you?";
const char *str2 = "hello parent environment! I'm good.";

#define TEMP_ADDR       ((char *)0xa00000)
#define TEMP_ADDR_CHILD ((char *)0xb00000)

void test_ipc_timed_region(uint64_t send_timeout, uint64_t recv_timeout, uint64_t send_sleep, uint64_t recv_sleep) {
    envid_t who;

    if ((who = fork()) == 0) {
        /* Child */
        size_t sz = PAGE_SIZE;
        sys_sleep(recv_sleep);
        int res = ipc_recv_timed(&who, TEMP_ADDR_CHILD, &sz, 0, recv_timeout);
        if (!who) {
            cprintf("%x: Error in ipc_recv_timed: %i\n", sys_getenvid(), res);
            sys_env_destroy(0);
        }
        cprintf("%x got message: %s\n", who, TEMP_ADDR_CHILD);
        if (strncmp(TEMP_ADDR_CHILD, str1, strlen(str1)) == 0)
            cprintf("child received correct message\n");
        sys_env_destroy(0);
    }
    sys_sleep(send_sleep);
    /* Parent */
    int res = sys_alloc_region(thisenv->env_id, TEMP_ADDR, PAGE_SIZE, PROT_RW);
    memcpy(TEMP_ADDR, str1, strlen(str1) + 1);
    res = ipc_send_timed(who, 0, TEMP_ADDR, PAGE_SIZE, PROT_RW, send_timeout);
    if (res < 0)
        cprintf("%x: Error in ipc_send_timed: %i\n", sys_getenvid(), res);
    wait(who);
    return;
}

void
umain(int argc, char **argv) {
    cprintf("Test recv after send:\n");
    test_ipc_timed(2000, 2000, 200, 1000);
    cprintf("\nTest send after recv:\n");
    test_ipc_timed(2000, 2000, 1000, 200);

    cprintf("\nTest recv timeout:\n");
    test_ipc_timed(1000, 1000, 2000, 0);
    cprintf("\nTest send timeout:\n");
    test_ipc_timed(1000, 1000, 0, 2000);

    cprintf("\nTest recv region after send:\n");
    test_ipc_timed_region(2000, 2000, 200, 1000);
    cprintf("\nTest send region after recv:\n");
    test_ipc_timed_region(2000, 2000, 1000, 200);

    cprintf("\nTest recv region timeout:\n");
    test_ipc_timed_region(1000, 1000, 2000, 0);
    cprintf("\nTest send region timeout:\n");
    test_ipc_timed_region(1000, 1000, 0, 2000);

    cprintf("\nTest recv nowait success:\n");
    test_ipc_timed(2000, 0, 0, 1000);
    cprintf("\nTest send nowait success:\n");
    test_ipc_timed(0, 2000, 1000, 0);

    cprintf("\nTest recv nowait timeout:\n");
    test_ipc_timed(1000, 0, 500, 0);
    cprintf("\nTest send nowait timeout:\n");
    test_ipc_timed(0, 1000, 0, 500);

    cprintf("\nTest send to timed:\n");
    test_ipc_send_to_timed(1000, 500, 0);
    cprintf("\nTest recv from timed:\n");
    test_ipc_recv_from_timed(1000, 200, 500);

    cprintf("\nPong time!!\n");
    timed_pong(200, 200);
}