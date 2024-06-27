#include "thread.h"
#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/thread.h>
#include <kern/monitor.h>
#include <kern/traceopt.h>


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
        //if (trace_sched)
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
                if (read_tsc() - envs[i].last_period_start_moment > envs[i].deadline) {
                    //  Drop him to the usual state
                    envs[i].env_class = ENV_CLASS_USUAL;
                    envs[i].env_deadline_exceed_handler();
                    break;
                }

                //  if the real-time process starts a new iteration
                if (read_tsc() - envs[i].last_period_start_moment > envs[i].period) {
                    envs[i].last_period_start_moment += envs[i].period;
                    envs[i].left_max_job_time = envs[i].max_job_time;
                    envs[i].env_status = ENV_RUNNABLE;
                }
                
                if (envs[i].left_max_job_time == 0)
                    envs[i].env_status = ENV_NOT_RUNNABLE;
                else
                    run_time_indices[rt_ind++] = i;
                break;

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
