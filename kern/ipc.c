#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/thread.h>
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
    env->env_ipc_thr = 0;
}

int
ipc_send(struct Env* env_recv, struct Env* env_send, uint32_t value, uintptr_t srcva, size_t size, int perm) {

    struct Thr *thr_recv, *thr_send;

    int res = thrid2thr(env_recv->env_ipc_thr, &thr_recv);
    if (res < 0)
        return -E_IPC_BAD_RECVER;

    res = thrid2thr(env_send->env_ipc_thr, &thr_send);
    if (res < 0)
        return -E_IPC_BAD_SENDER;

    env_recv->env_ipc_recving = false;
    env_send->env_ipc_sending = false;
    env_recv->env_ipc_timeout = 0;
    env_send->env_ipc_timeout = 0;
    env_recv->env_status = ENV_RUNNABLE;
    env_send->env_status = ENV_RUNNABLE;

    thr_recv->thr_status = THR_RUNNABLE;
    thr_send->thr_status = THR_RUNNABLE;
    thr_recv->thr_blocking_status = THR_NOT_WAITING;
    thr_send->thr_blocking_status = THR_NOT_WAITING;

    if (srcva < MAX_USER_ADDRESS && env_recv->env_ipc_dstva < MAX_USER_ADDRESS) {

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

    //cprintf ("1: %x trying to send page addr = %lx, size = %lx\n", curenv->env_id, srcva, size);

    curenv->env_ipc_sending = true;
    curenv->env_status = ENV_NOT_RUNNABLE;
    curthr->thr_status = THR_NOT_RUNNABLE;
    curthr->thr_blocking_status = THR_WAITING_IPC;
    curenv->env_ipc_timeout = read_tsc() + timeout * get_cpu_freq("hpet0") / 1000;

    curenv->env_ipc_value = value;
    curenv->env_ipc_to = envid;
    curenv->env_ipc_srcva = srcva;
    curenv->env_ipc_maxsz = size;
    curenv->env_ipc_perm = perm;

    curenv->env_ipc_thr = curthr->thr_id;
}

int 
process_timed_ipc(struct Env* env) {
    if (!env || env->env_status != ENV_NOT_RUNNABLE || !(env->env_ipc_sending || env->env_ipc_recving)) {
        return -E_BAD_ENV;
    }
    
    if (env->env_ipc_timeout == 0) {
        return -E_BAD_ENV;
    }

    struct Thr* thr;
    int res = thrid2thr(env->env_ipc_thr, &thr);
    if (res < 0)
        return -E_BAD_THR;

    if (read_tsc() >= env->env_ipc_timeout) {
        env->env_status = ENV_RUNNABLE;
        thr->thr_status = THR_RUNNABLE;
        thr->thr_blocking_status = THR_NOT_WAITING;
        clear_ipc(env);
        thr->thr_tf.tf_regs.reg_rax = -E_TIMEOUT;
        return -E_TIMEOUT;
    }

    if (env->env_ipc_recving)
        return 0;
    
    struct Env *env_recv;
    if (envid2env(env->env_ipc_to, &env_recv, 0)) {
        env->env_status = ENV_RUNNABLE;
        thr->thr_status = THR_RUNNABLE;
        thr->thr_blocking_status = THR_NOT_WAITING;
        clear_ipc(env);
        thr->thr_tf.tf_regs.reg_rax = -E_BAD_ENV;
        return -E_BAD_ENV;
    }
    if (env_recv->env_status == ENV_FREE || env_recv->env_status == ENV_DYING) {
        env->env_status = ENV_RUNNABLE;
        thr->thr_status = THR_RUNNABLE;
        thr->thr_blocking_status = THR_NOT_WAITING;
        clear_ipc(env);
        thr->thr_tf.tf_regs.reg_rax = -E_BAD_ENV;
        return -E_BAD_ENV;
    }
    if (!env_recv->env_ipc_recving)
        return -E_IPC_NOT_RECV;
    thr->thr_tf.tf_regs.reg_rax = ipc_send(env_recv, env, env->env_ipc_value, env->env_ipc_srcva, env->env_ipc_maxsz, env->env_ipc_perm);
    return thr->thr_tf.tf_regs.reg_rax;
}