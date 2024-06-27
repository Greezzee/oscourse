//  simple_change_class_test
#include <inc/lib.h>

void
umain(int argc, char **argv) {
    const char *env_classes[] = {"USUAL", "RUNTIME"};
    cprintf("i am environment %08x with class %s\n", thisenv->env_id, env_classes[thisenv->env_class]);
    sys_env_change_class(thisenv->env_id, ENV_CLASS_REAL_TIME, 1e10, 5 * 1e9, 1e9);
    cprintf("And now i am environment %08x with class %s\n", thisenv->env_id, env_classes[thisenv->env_class]);
    cprintf("Period = %lu, deadline = %lu, max_job_time = %lu\n", thisenv->period, thisenv->deadline, thisenv->max_job_time);
    cprintf("Exiting...\n");
    return;
}