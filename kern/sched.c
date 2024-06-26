#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/thread.h>
#include <kern/monitor.h>
#include <kern/traceopt.h>

struct Taskstate cpu_ts;
_Noreturn void sched_halt(void);

/* Choose a user environment to run and run it */
_Noreturn void
sched_yield(void) {
    /* Implement simple round-robin scheduling.
     *
     * Search through 'envs' for an ENV_RUNNABLE environment in
     * circular fashion starting just after the env was
     * last running.  Switch to the first such environment found.
     *
     * If no envs are runnable, but the environment previously
     * running is still ENV_RUNNING, it's okay to
     * choose that environment.
     *
     * If there are no runnable environments,
     * simply drop through to the code
     * below to halt the cpu */

    struct Env *next_env = curenv ? curenv : envs - 1;

    if (curthr && curthr->thr_next && curthr->thr_next->thr_status == THR_RUNNABLE) {
        if (trace_thread) {
            cprintf("[sched] Running next thr: %016lx; in-env id: %08lx\n", curthr->thr_next->thr_id, THR_ENVX(curthr->thr_next->thr_id));
            cprintf("[sched] thread's stack is at %016llx to %016lx\n", curthr->thr_next->thr_tf.tf_rsp - USER_STACK_SIZE, curthr->thr_next->thr_tf.tf_rsp);
        }
        thr_run(curthr->thr_next);
    }

    for (int32_t i = 0; i < NENV; ++i) {
        next_env++;
        if (next_env == envs + NENV) {
            next_env = envs;
        }
        if (next_env->env_status == ENV_RUNNABLE) {
            break;
        }
    }

    if (next_env->env_status == ENV_RUNNABLE) {
        env_run(next_env);
    } else if (next_env->env_status == ENV_RUNNING) {
        assert(next_env == curenv);
        env_run(next_env);
    }

    cprintf("Halt\n");

    /* No runnable environments,
     * so just halt the cpu */
    sched_halt();
}

/* Halt this CPU when there is nothing to do. Wait until the
 * timer interrupt wakes it up. This function never returns */
_Noreturn void
sched_halt(void) {

    /* For debugging and testing purposes, if there are no runnable
     * environments in the system, then drop into the kernel monitor */
    int i;
    for (i = 0; i < NENV; i++)
        if (envs[i].env_status == ENV_RUNNABLE ||
            envs[i].env_status == ENV_RUNNING) break;
    if (i == NENV) {
        cprintf("No runnable environments in the system!\n");
        for (;;) monitor(NULL);
    }

    /* Mark that no environment is running on CPU */
    curenv = NULL;

    /* Reset stack pointer, enable interrupts and then halt */
    asm volatile(
            "movq $0, %%rbp\n"
            "movq %0, %%rsp\n"
            "pushq $0\n"
            "pushq $0\n"
            "sti\n"
            "hlt\n" ::"a"(cpu_ts.ts_rsp0));

    /* Unreachable */
    for (;;)
        ;
}
