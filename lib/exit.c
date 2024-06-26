
#include <inc/lib.h>

void
exit(void) {
    if (thisenv->env_thr_count == 1)
        close_all();
    sys_thr_exit();
}
