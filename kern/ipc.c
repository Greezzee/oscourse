#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/timer.h>
#include <kern/pmap.h>
#include <kern/sched.h>

void
clear_ipc(struct Env* env) {
    env->env_ipc_timeout = 0;
    env->env_ipc_recving = false;
    env->env_ipc_sending = false;
    env->env_ipc_from = 0;
    env->env_ipc_to = 0;
    env->env_ipc_value = 0;
    env->env_ipc_dstva = 0;
    env->env_ipc_srcva = 0;
    env->env_ipc_maxsz = 0;
    env->env_ipc_perm = 0;
}

int
ipc_send(struct Env* env_recv, struct Env* env_send, uint32_t value, uintptr_t srcva, size_t size, int perm) {
    env_recv->env_ipc_recving = false;
    env_send->env_ipc_sending = false;
    env_recv->env_ipc_timeout = 0;
    env_send->env_ipc_timeout = 0;
    env_recv->env_status = ENV_RUNNABLE;
    env_send->env_status = ENV_RUNNABLE;

    if (srcva < MAX_USER_ADDRESS && env_recv->env_ipc_dstva < MAX_USER_ADDRESS) {
        if (PAGE_OFFSET(srcva) ||
            PAGE_OFFSET(env_recv->env_ipc_dstva) ||
            perm & ~(ALLOC_ONE | ALLOC_ZERO | PROT_ALL))
            return -E_INVAL;
        if ((perm & PROT_W) && user_mem_check(env_send, (void *)srcva, size, PROT_W) < 0)
            return -E_INVAL;

        size_t actual_size = MIN(size, env_recv->env_ipc_maxsz);
        if (map_region(&env_recv->address_space, env_recv->env_ipc_dstva, &env_send->address_space, srcva, actual_size, perm | PROT_USER_)) {
            return -E_NO_MEM;
        }

        env_recv->env_ipc_maxsz = actual_size;
        env_send->env_ipc_maxsz = actual_size;
        env_recv->env_ipc_perm = perm;
    } else {
        env_recv->env_ipc_perm = 0;
    }
    env_recv->env_ipc_value = value;
    env_recv->env_ipc_from = env_send->env_id;
    return 0;
}

void
ipc_prepare_send_timed(envid_t envid, uint32_t value, uintptr_t srcva, size_t size, int perm, uint64_t timeout) {
    curenv->env_ipc_sending = true;
    curenv->env_status = ENV_NOT_RUNNABLE;
    curenv->env_ipc_timeout = read_tsc() + timeout * get_cpu_freq("hpet0") / 1000;

    curenv->env_ipc_to = envid;
    curenv->env_ipc_srcva = srcva;
    curenv->env_ipc_maxsz = size;
    curenv->env_ipc_perm = perm;
}

int 
process_timed_ipc(struct Env* env) {
    if (!env || env->env_status != ENV_NOT_RUNNABLE || !(env->env_ipc_sending || env->env_ipc_recving))
        return -E_BAD_ENV;
    
    if (env->env_ipc_timeout == 0)
        return -E_BAD_ENV;

    if (read_tsc() >= env->env_ipc_timeout) {
        env->env_status = ENV_RUNNABLE;
        clear_ipc(env);
        env->env_tf.tf_regs.reg_rax = -E_TIMEOUT;
        cprintf("Timeout!!!\n");
        return -E_TIMEOUT;
    }

    if (env->env_ipc_recving)
        return 0;
    
    struct Env *env_recv;
    if (envid2env(env->env_ipc_to, &env_recv, 0)) {
        env->env_status = ENV_RUNNABLE;
        clear_ipc(env);
        env->env_tf.tf_regs.reg_rax = -E_BAD_ENV;
        return -E_BAD_ENV;
    }
    if (env_recv->env_status != ENV_NOT_RUNNABLE) {
        env->env_status = ENV_RUNNABLE;
        clear_ipc(env);
        env->env_tf.tf_regs.reg_rax = -E_BAD_ENV;
        return -E_BAD_ENV;
    }
    if (!env_recv->env_ipc_recving)
        return -E_IPC_NOT_RECV;

    return ipc_send(env_recv, env, env->env_ipc_value, env->env_ipc_srcva, env->env_ipc_maxsz, env->env_ipc_perm);
}