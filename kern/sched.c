#include "thread.h"
#include "pmap.h"
#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/thread.h>
#include <kern/monitor.h>
#include <kern/traceopt.h>
#include <inc/string.h>

struct Taskstate cpu_ts;
_Noreturn void sched_halt(void);

int64_t get_sheduler_metric(struct Env* env) {
    int64_t left_time_to_deadline = (int64_t)env->last_period_start_moment + (int64_t)(env->deadline) - (int64_t)read_tsc();
    if (trace_sched) {
        cprintf("last_period_start_moment = %ld\n", (int64_t)env->last_period_start_moment);
    }
    int64_t metric = left_time_to_deadline - (int64_t)env->left_max_job_time;
    return metric > 0 ? metric : 0;
}

void insertion_sort(int32_t n, int32_t ind_arr[], struct Env *envs)
{
    int32_t newElement, location;

    for (int32_t i = 1; i < n; i++)
    {
        newElement = ind_arr[i];
        location = i - 1;

        while(location >= 0 && get_sheduler_metric(&envs[ind_arr[location]]) > get_sheduler_metric(&envs[newElement]))
        {
            ind_arr[location+1] = ind_arr[location];
            location = location - 1;
        }
        ind_arr[location+1] = newElement;
    }
}

/* Choose a thread in certain environment to run */
struct Thr*
sched_thr_yield(struct Env* env) {
    if (env == NULL)
        return NULL;

    if (env->env_class == ENV_CLASS_USUAL) {
        if (trace_sched)
            cprintf("Processing usual process in sched_thr_yield...\n");

        struct Thr* cur_thr;
        if (curthr == NULL) {
            int res = thrid2thr(env->env_thr_cur, &cur_thr);
            if (res < 0)
                panic("Running bad thr\n");
        }
        else {
            cur_thr = curthr;
        }
        
        struct Thr* possible_next_thr = cur_thr->thr_next;
        
        if (!possible_next_thr)
            thrid2thr(env->env_thr_head, &possible_next_thr);

        while (possible_next_thr != cur_thr) {     
            if (possible_next_thr->thr_status == THR_NOT_RUNNABLE)
                thr_process_not_runnable(possible_next_thr);

            if (possible_next_thr->thr_status == THR_RUNNABLE)
                return possible_next_thr;
            
            possible_next_thr = possible_next_thr->thr_next;

            if (!possible_next_thr)
                thrid2thr(env->env_thr_head, &possible_next_thr);
        }
        return NULL;
    }
    else {
        if (trace_sched)
        cprintf("Processing real-time process in sched_thr_yield...\n");

        struct Thr* possible_next_thr = NULL;
        thrid2thr(env->env_thr_head, &possible_next_thr);

        struct Thr* next_thread = NULL;

        uint32_t max_priority = 0;
        while (possible_next_thr != NULL) {
            if (possible_next_thr->fixed_priority > max_priority) {
                max_priority = possible_next_thr->fixed_priority;

                if (possible_next_thr->thr_status == THR_NOT_RUNNABLE)
                    thr_process_not_runnable(possible_next_thr);

                if (possible_next_thr->thr_status == THR_RUNNABLE)
                    next_thread = possible_next_thr;
                
                possible_next_thr = possible_next_thr->thr_next;
            }
        }

        return next_thread;
    }
}

static void
exceed_deadline_handler(struct Trapframe *tf) {
    uintptr_t cr2 = rcr2();

    /* We've already handled kernel-mode exceptions, so if we get here,
     * the page fault happened in user mode.
     *
     * Call the environment's page fault upcall, if one exists.  Set up a
     * page fault stack frame on the user exception stack (below
     * USER_EXCEPTION_STACK_TOP), then branch to curenv->env_pgfault_upcall.
     *
     * The page fault upcall might cause another page fault, in which case
     * we branch to the page fault upcall recursively, pushing another
     * page fault stack frame on top of the user exception stack.
     *
     * The trap handler needs one word of scratch space at the top of the
     * trap-time stack in order to return.  In the non-recursive case, we
     * don't have to worry about this because the top of the regular user
     * stack is free.  In the recursive case, this means we have to leave
     * an extra word between the current top of the exception stack and
     * the new stack frame because the exception stack _is_ the trap-time
     * stack.
     *
     * If there's no page fault upcall, the environment didn't allocate a
     * page for its exception stack or can't write to it, or the exception
     * stack overflows, then destroy the environment that caused the fault.
     * Note that the grade script assumes you will first check for the page
     * fault upcall and print the "user fault va" message below if there is
     * none.  The remaining three checks can be combined into a single test.
     *
     * Hints:
     *   user_mem_assert() and env_run() are useful here.
     *   To change what the user environment runs, modify 'curenv->env_tf'
     *   (the 'tf' variable points at 'curenv->env_tf'). */


    static_assert(UTRAP_RIP == offsetof(struct UTrapframe, utf_rip), "UTRAP_RIP should be equal to RIP offset");
    static_assert(UTRAP_RSP == offsetof(struct UTrapframe, utf_rsp), "UTRAP_RSP should be equal to RSP offset");

    if (!curenv->exceed_deadline_upcall) {
        return; //  do nothing, this is not neccesary to have a handler
    }
    /* Force allocation of exception stack page to prevent memcpy from
     * causing pagefault during another pagefault */
    // LAB 9: Your code here:

    uint32_t thr_low_id = THR_ENVX(curthr->thr_id);
    uintptr_t user_exception_stack_top_cur_thread = USER_EXCEPTION_STACK_TOP - thr_low_id * PAGE_SIZE;

    force_alloc_page(&curenv->address_space, user_exception_stack_top_cur_thread - PAGE_SIZE, PAGE_SIZE);

    /* Assert existance of exception stack */
    // LAB 9: Your code here:

    uintptr_t ursp;
    if (tf->tf_rsp < user_exception_stack_top_cur_thread && tf->tf_rsp >= user_exception_stack_top_cur_thread - PAGE_SIZE) {
        ursp = tf->tf_rsp - sizeof(uintptr_t);
    } else {
        ursp = user_exception_stack_top_cur_thread;
    }

    ursp -= sizeof(struct UTrapframe);
    user_mem_assert(curenv, (void *)ursp, sizeof(struct UTrapframe), PROT_W);

    /* Build local copy of UTrapframe */
    // LAB 9: Your code here:

    struct UTrapframe utf;
    utf.utf_fault_va = cr2;
    utf.utf_err = tf->tf_err;
    utf.utf_regs = tf->tf_regs;
    utf.utf_rip = tf->tf_rip;
    utf.utf_rflags = tf->tf_rflags;
    utf.utf_rsp = tf->tf_rsp;
    tf->tf_rsp = ursp;
    tf->tf_rip = (uintptr_t)curenv->exceed_deadline_upcall;

    /* And then copy it userspace (nosan_memcpy()) */
    // LAB 9: Your code here:

    struct AddressSpace *old = switch_address_space(&curenv->address_space);
    set_wp(0);
    nosan_memcpy((void *)ursp, (void *)&utf, sizeof(struct UTrapframe));
    set_wp(1);
    switch_address_space(old);

    /* Rerun current environment */
    // LAB 9: Your code here:

    env_run(curenv, NULL);

    while (1)
        ;
}

/* Choose a user environment to run and run it */
_Noreturn void
sched_env_yield(void) {
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

    if (trace_sched)
        cprintf("\n\n\n-------------scheduler-iteration--------------\n");

    // if (curthr && curthr->thr_next && curthr->thr_next->thr_status == THR_RUNNABLE) {
    //     thr_run(curthr->thr_next);
    // }

    int32_t run_time_indices[NENV];
    int32_t rt_ind = 0;
    
    for (int32_t i = 0; i < NENV; ++i) if (envs[i].env_status != ENV_FREE) {
        switch (envs[i].env_class) {
            case ENV_CLASS_REAL_TIME:
                //  if the process reached the deadline and didn't finish
                if (read_tsc() - envs[i].last_period_start_moment > envs[i].deadline && envs[i].env_status != ENV_PERIODIC_WAITING) {
                    //  Drop him to the usual state
                    envs[i].env_class = ENV_CLASS_USUAL;
                    struct Thr* head_thr = NULL;
                    thrid2thr(envs[i].env_thr_head, &head_thr);
                    if (head_thr)
                        exceed_deadline_handler(&head_thr->thr_tf);
                    break;
                }

                //  if the real-time process starts a new iteration
                if ((envs[i].last_period_start_moment == 0 || read_tsc() - envs[i].last_period_start_moment > envs[i].period) &&
                     envs[i].env_status == ENV_PERIODIC_WAITING) {
                    envs[i].last_period_start_moment = read_tsc();
                    envs[i].left_max_job_time = envs[i].max_job_time;
                    envs[i].env_status = ENV_RUNNABLE;
                }
                
                run_time_indices[rt_ind++] = i;

            case ENV_CLASS_USUAL:
                break;
        }
    }

    if (trace_sched) {
        cprintf("Run_time_indices before sort:\n");
        for (int32_t i = 0; i < rt_ind; ++i) {
            cprintf("%d\n", run_time_indices[i]);
        }
    }

    insertion_sort(rt_ind, run_time_indices, envs);
    
    if (trace_sched) {
        cprintf("Run_time_indices after sort:\n");
        for (int32_t i = 0; i < rt_ind; ++i) {
            cprintf("ind = %d and id = [%08X] with metric = %ld and left_max_job_time = %lu\n", run_time_indices[i], envs[run_time_indices[i]].env_id, get_sheduler_metric(&envs[run_time_indices[i]]), envs[run_time_indices[i]].left_max_job_time);
        }
        cprintf("tsc = %ld\n", read_tsc());
    }

    struct Env *next_env = envs;

    uint8_t has_rt_process_for_run = 0;

    for (int32_t i = 0; i < rt_ind; ++i) {
        next_env = &envs[run_time_indices[i]];
        if (trace_sched)
            cprintf("considering process with id = [%08X]...\n", next_env->env_id);
        if (next_env->env_status == ENV_NOT_RUNNABLE) {
            env_process_not_runnable(next_env);
        }
        if (next_env->env_status == ENV_RUNNABLE || next_env->env_status == ENV_RUNNING) {
            has_rt_process_for_run = 1;
            break;
        }
    }

    if (!has_rt_process_for_run) {
        next_env = curenv ? curenv : envs - 1;

        for (int32_t i = 0; i < NENV; ++i) {
            next_env++;
            if (next_env == envs + NENV) {
                next_env = envs;
            }
            if (next_env->env_status == ENV_NOT_RUNNABLE && next_env->env_class == ENV_CLASS_USUAL) {
                env_process_not_runnable(next_env);
            }
            if (next_env->env_class == ENV_CLASS_USUAL && next_env->env_status == ENV_RUNNABLE) {
                break;
            }
        }
    }

    if (trace_sched)
        cprintf("Next process for run: [%08X]\n", next_env->env_id);

    if (trace_sched)
        cprintf("Start sched_thr_yield in sched_env_yield...\n");
    struct Thr* next_thread = sched_thr_yield(next_env);
    if (trace_sched)
        cprintf("End sched_thr_yield in sched_env_yield, next_possible_thread = %p\n", next_thread);

    if (next_env->env_status == ENV_RUNNABLE) {
        env_run(next_env, next_thread);
    } else if (next_env->env_status == ENV_RUNNING) {
        assert(next_env == curenv);
        env_run(next_env, next_thread);
    }

    cprintf("Halt\n");

    /* No runnable environments,
     * so just halt the cpu */
    sched_halt();
}

_Noreturn void
sched_yield(void) {
    if (trace_sched)
        cprintf("Starting sched_yield...\n");
    if (!curenv || !curthr) {
        if (trace_sched)
            cprintf("No curenv or curthr, starting sched_env_yield...\n");
        sched_env_yield();
    }
    
    if (curenv->env_status != ENV_RUNNING) {
        if (trace_sched)
            cprintf("Curenv is not running, starting sched_env_yield...\n");
        sched_env_yield();
    }
    
    // if (!possible_next_thr)
    //     thrid2thr(curenv->env_thr_head, &possible_next_thr);

    // while (possible_next_thr != curthr) {     
    //     if (possible_next_thr->thr_status == THR_NOT_RUNNABLE)
    //         thr_process_not_runnable(possible_next_thr);

    //     if (possible_next_thr->thr_status == THR_RUNNABLE)
    //         thr_run(possible_next_thr);
        
    //     possible_next_thr = possible_next_thr->thr_next;

    //     if (!possible_next_thr)
    //         thrid2thr(curenv->env_thr_head, &possible_next_thr);
    // }

    struct Thr* possible_next_thr = sched_thr_yield(curenv);
    if (possible_next_thr != NULL) {
        if (trace_sched)
            cprintf("Found thread for launch in sched_yield, running...\n");
        thr_run(possible_next_thr);
    }
    else {
        if (trace_sched)
            cprintf("Not found thread for launch in sched_yield, running sched_env_yield...\n");
        sched_env_yield();
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
