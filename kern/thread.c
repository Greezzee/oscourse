#include "thread.h"
#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>
#include <inc/vsyscall.h>

#include <kern/traceopt.h>
#include <kern/pmap.h>
#include <kern/env.h>
#include <kern/thread.h>
#include <kern/sched.h>
#include <kern/mutex.h>

struct Thr *thrs = NULL;
struct Thr *curthr = NULL;

static struct Thr *thr_free_list;

// should be bigger than LOG2NTHR + LOG2NTHR_PER_ENV
#define THRGENSHIFT 23

int thrid2thr(thrid_t thrid, struct Thr **thr_store) {
    struct Thr *thr;

    /* If envid is zero, return the current environment. */
    if (!thrid) {
        *thr_store = curthr;
        return 0;
    }
    thr = &thrs[THRX(thrid)];
    if (thr->thr_status == THR_FREE || thr->thr_id != thrid) {
        *thr_store = NULL;
        return -E_BAD_THR;
    }

    *thr_store = thr;
    return 0;
}

void thr_init() {
    assert(current_space);
    thrs = kzalloc_region(NTHR * sizeof(*thrs));
    memset(thrs, 0, ROUNDUP(NTHR * sizeof(*thrs), PAGE_SIZE));
    map_region(current_space, UTHRS, &kspace, (uintptr_t)thrs, UTHRS_SIZE, PROT_R | PROT_USER_);
    thr_free_list = &thrs[0];
	thrs[0].thr_id = 0;
	for (int i = 1; i < NTHR; i++) {
		thrs[i - 1].thr_next_free =  &thrs[i];
		thrs[i].thr_id = i;
	}
	thrs[NTHR - 1].thr_next_free = NULL;
}

int thr_alloc(struct Thr **pthr, struct Env* env, size_t low_id) {

    struct Thr *thr;
    if (!(thr = thr_free_list))
        return -E_NO_FREE_THR;
    (*pthr) = thr;

    /* Generate an env_id for this environment */
    int32_t generation = (int32_t)(thr->thr_id + (1 << THRGENSHIFT));
    generation &= ~((1 << (LOG2NTHR + LOG2NTHR_PER_ENV)) - 1);
    /* Don't create a negative thr_id */
    if (generation <= 0) generation = 1 << THRGENSHIFT;
    generation = generation | (thr - thrs);
    // mapping stack for this thr
    int res = map_region(&env->address_space, USER_STACK_TOP - (low_id + 1) * USER_STACK_SIZE, NULL, 0, USER_STACK_SIZE, PROT_R | PROT_W | PROT_USER_ | ALLOC_ZERO);
    if (res < 0) {
        cprintf("Allocating new thr: Region not mapped: %i\n", res);
        return res;
    }

    (*pthr)->thr_id = (uint64_t)generation | (low_id << LOG2NTHR) | (((uint64_t)env->env_id) << 32);
    (*pthr)->thr_env = env->env_id;

    (*pthr)->thr_runs = 0;
    (*pthr)->thr_stack_top = USER_STACK_TOP - low_id * USER_STACK_SIZE;
    (*pthr)->thr_status = THR_RUNNABLE;
    thr_free_list = (*pthr)->thr_next_free;

    memset(&(*pthr)->thr_tf, 0, sizeof((*pthr)->thr_tf));

#ifdef CONFIG_KSPACE
    (*pthr)->thr_tf.tf_ds = GD_KD;
    (*pthr)->thr_tf.tf_es = GD_KD;
    (*pthr)->thr_tf.tf_ss = GD_KD;
    (*pthr)->thr_tf.tf_cs = GD_KT;

    static uintptr_t stack_top = 0x2000000;
    (*pthr)->thr_tf.tf_rsp = stack_top;
    stack_top += 2 * PAGE_SIZE;
#else

    (*pthr)->thr_tf.tf_ds = GD_UD | 3;
    (*pthr)->thr_tf.tf_es = GD_UD | 3;
    (*pthr)->thr_tf.tf_ss = GD_UD | 3;
    (*pthr)->thr_tf.tf_cs = GD_UT | 3;
    (*pthr)->thr_tf.tf_rsp = (*pthr)->thr_stack_top;

#endif

    (*pthr)->thr_tf.tf_rflags = FL_IF | (env->env_type == ENV_TYPE_FS ? FL_IOPL_3 : FL_IOPL_0);

    return 0;
}

void thr_free(struct Thr *thr) {
    if (trace_thread)
        cprintf("Destroing thread: %016lx; in-env id: %08lx\n", thr->thr_id, THR_ENVX(thr->thr_id));
    thr->thr_status = THR_FREE;
    struct Env* env;
    int res = envid2env(thr->thr_env, &env, 0);
    if (res < 0)
        panic("Freeing thread of unexisting env\n");
    unmap_region(&env->address_space, thr->thr_stack_top - USER_STACK_SIZE, USER_STACK_SIZE);

    thr->thr_next_free = thr_free_list;
    thr_free_list = thr;
    env->env_thr_count--;
}

int thr_create_with_priority(envid_t envid, uint32_t force_thr_env_id, struct Thr** created_thr, uint32_t fixed_priority) {
    struct Env* env;
    int res = envid2env(envid, &env, 0);
    if (res < 0)
        return -E_BAD_ENV;

    struct Thr* new_thr;
    if (env->env_thr_count == 0) {// first thread of the env
        if (force_thr_env_id >= NTHR_PER_ENV)
            force_thr_env_id = 0;
        int res = thr_alloc(&new_thr, env, force_thr_env_id);
        if (res < 0) {
            return res;
        }
        env->env_thr_head = new_thr->thr_id;
        env->env_thr_count++;
        new_thr->thr_env = env->env_id;
        new_thr->thr_next = NULL;
        new_thr->fixed_priority = fixed_priority; 
        if (created_thr)
            *created_thr = new_thr;
        if (trace_thread) {
            cprintf("Created new head thread: %016lx; in-env id: %08lx\n", new_thr->thr_id, THR_ENVX(new_thr->thr_id));
            cprintf("New thread's stack is at %016llx to %016lx\n", new_thr->thr_tf.tf_rsp - USER_STACK_SIZE, new_thr->thr_tf.tf_rsp);
        }
        return 0;
    }

    struct Thr* cur_thr;
    res = thrid2thr(env->env_thr_head, &cur_thr);
    if (res < 0)
        return res;

    char ustack_map[NTHR_PER_ENV] = {};
    while (cur_thr && cur_thr->thr_next) {       
        ustack_map[THR_ENVX(cur_thr->thr_id)] = 1;
        cur_thr = cur_thr->thr_next;
    }

    if (cur_thr)
        ustack_map[THR_ENVX(cur_thr->thr_id)] = 1;

    size_t i = 0;
    if (force_thr_env_id < NTHR_PER_ENV && !ustack_map[force_thr_env_id]) // check if we can create thread with non-generated id
        i = force_thr_env_id;
    else if (force_thr_env_id < NTHR_PER_ENV) // we can't
        return -E_BAD_THR;
    else
        for (; i < NTHR_PER_ENV && ustack_map[i]; i++); // searching for closest free stack segment

    if (i == NTHR_PER_ENV) 
        return -E_NO_FREE_THR;

    thr_alloc(&(cur_thr->thr_next), env, i);
    if (res < 0)
        return res;
    new_thr = cur_thr->thr_next;

    env->env_thr_count++;
    new_thr->thr_env = env->env_id;
    new_thr->thr_next = NULL;
    if (created_thr)
        *created_thr = new_thr;
    if (trace_thread) {
        cprintf("Created new non-head thread: %016lx; in-env id: %08lx\n", new_thr->thr_id, THR_ENVX(new_thr->thr_id));
        cprintf("New thread's stack is at %016llx to %016lx\n", new_thr->thr_tf.tf_rsp - USER_STACK_SIZE, new_thr->thr_tf.tf_rsp);
    }
    return 0;
}

int thr_create(envid_t envid, uint32_t force_thr_env_id, struct Thr** created_thr) {
    return thr_create_with_priority(envid, force_thr_env_id, created_thr, DEFAULT_PRIORITY);
}

int thr_destroy(thrid_t thrid) {

    struct Thr *thr;
    int res = thrid2thr(thrid, &thr);
    if (res < 0)
        return res;

    struct Env* env;
    res = envid2env(thr->thr_env, &env, 0);
    if (res < 0)
        return res;

    if (env->env_thr_head == thr->thr_id && !thr->thr_next) { // last thread
        thr_free(thr);
        env_destroy(env);
        return 0;
    }
    else if (env->env_thr_head == thr->thr_id) {
        env->env_thr_head = thr->thr_next->thr_id;
        thr_free(thr);
        if (env->env_thr_cur == thr->thr_id) {
            env->env_thr_cur = thr->thr_next->thr_id;
        }
        return 0;
    }

    struct Thr* prev_thr;
    res = thrid2thr(env->env_thr_head, &prev_thr);
    if (res < 0)
        return res;

    while (prev_thr && prev_thr->thr_next != thr)
        prev_thr = prev_thr->thr_next;
    
    if (!prev_thr)
        return -E_THR_NOT_EXISTS;

    if (env->env_thr_cur == thr->thr_id) {
        if (!thr->thr_next)
            env->env_thr_cur = env->env_thr_head;
        else
            env->env_thr_cur = thr->thr_next->thr_id;
    }

    prev_thr->thr_next = thr->thr_next;
    thr_free(thr);

    return 0;
}

/* Restores the register values in the Trapframe with the 'ret' instruction.
 * This exits the kernel and starts executing some environment's code.
 *
 * This function does not return.
 */

_Noreturn void
thr_pop_tf(struct Trapframe *tf) {
    asm volatile(
            "movq %0, %%rsp\n"
            "movq 0(%%rsp), %%r15\n"
            "movq 8(%%rsp), %%r14\n"
            "movq 16(%%rsp), %%r13\n"
            "movq 24(%%rsp), %%r12\n"
            "movq 32(%%rsp), %%r11\n"
            "movq 40(%%rsp), %%r10\n"
            "movq 48(%%rsp), %%r9\n"
            "movq 56(%%rsp), %%r8\n"
            "movq 64(%%rsp), %%rsi\n"
            "movq 72(%%rsp), %%rdi\n"
            "movq 80(%%rsp), %%rbp\n"
            "movq 88(%%rsp), %%rdx\n"
            "movq 96(%%rsp), %%rcx\n"
            "movq 104(%%rsp), %%rbx\n"
            "movq 112(%%rsp), %%rax\n"
            "movw 120(%%rsp), %%es\n"
            "movw 128(%%rsp), %%ds\n"
            "addq $152,%%rsp\n" /* skip tf_trapno and tf_errcode */
            "iretq" ::"g"(tf)
            : "memory");

    /* Mostly to placate the compiler */
    panic("Reached unrecheble\n");
}

_Noreturn void
thr_run(struct Thr *thr) {
    assert(thr);

    if (curthr != thr) 
    { // Context switch.
		if (curthr && curthr->thr_status == THR_RUNNING)
			curthr->thr_status = THR_RUNNABLE;
		curthr = thr;

        curenv->env_thr_cur = thr->thr_id;
		curthr->thr_status = THR_RUNNING;
		curthr->thr_runs++;
	}

    if (trace_thread) {
        cprintf("[sched] Running next thr: %016lx; in-env id: %08lx\n", curthr->thr_id, THR_ENVX(curthr->thr_id));
        cprintf("[sched] thread's stack is at %016llx to %016lx\n", curthr->thr_tf.tf_rsp - USER_STACK_SIZE, curthr->thr_tf.tf_rsp);
    }  

    thr_pop_tf(&curthr->thr_tf);
    // LAB 8: Your code here

    while (1)
        ;
}

#ifdef CONFIG_KSPACE
void
csys_yield(struct Trapframe *tf) {
    memcpy(&curthr->thr_tf, tf, sizeof(struct Trapframe));
    sched_yield();
}
#endif

void thr_process_not_runnable(struct Thr *thr) {
    if (thr->thr_status != THR_NOT_RUNNABLE)
        return;
    
    if (thr->thr_blocking_status == THR_WAITING_JOIN) {
        struct Thr* waited_thr;
        if (thrid2thr(thr->thr_block, &waited_thr) < 0) {
            thr->thr_status = THR_RUNNABLE;
            thr->thr_tf.tf_regs.reg_rax = 0;
        }
    }
    
    if (thr->thr_blocking_status == THR_WAITING_MUTEX) {
        struct Mutex* mutex;
        if (mutexid2mutex((mutexid_t)thr->thr_block, &mutex) < 0) {
            thr_destroy(thr->thr_id);
            return;
        }

        if (!mutex->mutex_is_locked) {
            thr->thr_status = THR_RUNNABLE;
            thr->thr_tf.tf_regs.reg_rax = 0;
        }
    }
}