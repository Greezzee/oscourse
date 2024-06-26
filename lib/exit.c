
#include <inc/lib.h>

void
exit(void) {
    //close_all();
    sys_thr_exit();
}
