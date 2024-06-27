#ifndef JOS_KERN_IPC_H
#define JOS_KERN_IPC_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

void clear_ipc(struct Env* env);
int process_timed_ipc(struct Env* env);
int ipc_send(struct Env* env_recv, struct Env* env_send, uint32_t value, uintptr_t srcva, size_t size, int perm);
void ipc_prepare_send_timed(envid_t envid, uint32_t value, uintptr_t srcva, size_t size, int perm, uint64_t timeout);

#endif /* !JOS_KERN_IPC_H */