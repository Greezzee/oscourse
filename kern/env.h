/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_ENV_H
#define JOS_KERN_ENV_H

#include <inc/env.h>

#define NCPU 1

/* All environments */
extern struct Env *envs;
/* Currently active environment */
extern struct Env *curenv;
extern struct Segdesc32 gdt[];

void env_init(void);
int env_alloc(struct Env **penv, envid_t parent_id, enum EnvType type, enum EnvClass env_class);
void env_free(struct Env *env);
void env_create(uint8_t *binary, size_t size, enum EnvType type, enum EnvClass env_class, uint64_t period, uint64_t deadline, uint64_t max_job_time, void* env_deadline_exceed_handler);
void env_destroy(struct Env *env);

int envid2env(envid_t envid, struct Env **env_store, bool checkperm);
void env_run(struct Env *e);
_Noreturn void env_pop_tf(struct Trapframe *tf);

#ifdef CONFIG_KSPACE
extern void sys_exit(void);
extern void sys_yield(void);
#endif

/* Without this extra macro, we couldn't pass macros like TEST to
 * ENV_CREATE because of the C pre-processor's argument prescan rule */
#define ENV_PASTE3(x, y, z) x##y##z

#define ENV_CREATE_KERNEL_TYPE(x)                                   \
    do {                                                            \
        extern uint8_t ENV_PASTE3(_binary_obj_, x, _start)[];       \
        extern uint8_t ENV_PASTE3(_binary_obj_, x, _end)[];         \
        env_create(ENV_PASTE3(_binary_obj_, x, _start),             \
                   ENV_PASTE3(_binary_obj_, x, _end) -              \
                           ENV_PASTE3(_binary_obj_, x, _start),     \
                   ENV_TYPE_KERNEL, ENV_CLASS_USUAL, 0, 0, 0, NULL);\
    } while (0)

#define ENV_CREATE(x, type)                                     \
    do {                                                        \
        extern uint8_t ENV_PASTE3(_binary_obj_, x, _start)[];   \
        extern uint8_t ENV_PASTE3(_binary_obj_, x, _end)[];     \
        env_create(ENV_PASTE3(_binary_obj_, x, _start),         \
                   ENV_PASTE3(_binary_obj_, x, _end) -          \
                           ENV_PASTE3(_binary_obj_, x, _start), \
                   type, ENV_CLASS_USUAL, 0, 0, 0, NULL);       \
    } while (0)

#define ENV_CREATE_DIFF_CLASS(x, type, env_class, period, deadline, max_job_time, env_deadline_exceed_handler)  \
    do {                                                                                                        \
        extern uint8_t ENV_PASTE3(_binary_obj_, x, _start)[];                                                   \
        extern uint8_t ENV_PASTE3(_binary_obj_, x, _end)[];                                                     \
        env_create(ENV_PASTE3(_binary_obj_, x, _start),                                                         \
                   ENV_PASTE3(_binary_obj_, x, _end) -                                                          \
                           ENV_PASTE3(_binary_obj_, x, _start),                                                 \
                   type, env_class, period, deadline, max_job_time, env_deadline_exceed_handler);                                            \
    } while (0)

#endif /* !JOS_KERN_ENV_H */
