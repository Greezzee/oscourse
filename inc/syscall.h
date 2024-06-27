#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

/* system call numbers */
enum {
    SYS_cputs = 0,
    SYS_cgetc,
    SYS_getenvid,
    SYS_env_destroy,
    SYS_alloc_region,
    SYS_map_region,
    SYS_map_physical_region,
    SYS_unmap_region,
    SYS_region_refs,
    SYS_exofork,
    SYS_env_set_status,
    SYS_env_set_trapframe,
    SYS_env_set_pgfault_upcall,
    SYS_env_set_exceed_deadline_upcall,
    SYS_env_change_class,
    SYS_yield,
    SYS_periodic_wait,
    SYS_ipc_try_send,
    SYS_ipc_recv,
    SYS_gettime,
    SYS_monitor,
    SYS_thr_create,
    SYS_thr_exit,
    SYS_thr_cancel,
    SYS_getthrid,
    SYS_thr_join,
    SYS_mutex_create,
    SYS_mutex_destroy,
    SYS_mutex_block_thr,
    SYS_mutex_unlock,
    NSYSCALLS
};

#endif /* !JOS_INC_SYSCALL_H */
