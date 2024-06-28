#include <inc/lib.h>

/* Assembly language pgfault entrypoint defined in lib/pfentry.S. */
extern void _dlex_upcall(void);

#define MAX_DLHANDLER 8

/* Vector of currently set handlers. */
dl_handler_t *_dlhandler_vec[MAX_DLHANDLER];
/* Vector size */
static size_t _dlhandler_off = 0;
static bool _dlhandler_inititiallized = 0;

void
_handle_deadline_exeeded(struct UTrapframe *utf) {
    /* This trying to handle pagefault until
     * some handler returns 1, that indicates
     * successfully handled exception */
    (void)utf;
    for (size_t i = 0; i < _dlhandler_off; i++)
        if (_dlhandler_vec[i]()) return;
}

/* Set the page fault handler function.
 * If there isn't one yet, _pgfault_handler will be 0.
 * The first time we register a handler, we need to
 * allocate an exception stack (one page of memory with its top
 * at USER_EXCEPTION_STACK_TOP), and tell the kernel to call the assembly-language
 * _pgfault_upcall routine when a page fault occurs. */
int
add_deadline_handler(dl_handler_t handler) {
    int res = 0;
    if (!_dlhandler_inititiallized) {
        /* First time through! */
        // LAB 9: Your code here:
        sys_alloc_region(sys_getenvid(), (void*)(USER_EXCEPTION_STACK_TOP - PAGE_SIZE * NTHR_PER_ENV), PAGE_SIZE * NTHR_PER_ENV, PROT_RW);
        _dlhandler_vec[_dlhandler_off++] = handler;
        _dlhandler_inititiallized = 1;
        res = sys_env_set_exceed_deadline_upcall(sys_getenvid(), _dlex_upcall);
        goto end;
    }

    for (size_t i = 0; i < _dlhandler_off; i++)
        if (handler == _dlhandler_vec[i]) return 0;

    if (_dlhandler_off == MAX_DLHANDLER)
        res = -E_INVAL;
    else
        _dlhandler_vec[_dlhandler_off++] = handler;

end:
    if (res < 0) panic("set_deadline_handler: %i", res);
    return res;
}

void
remove_deadline_handler(dl_handler_t handler) {
    assert(_dlhandler_inititiallized);
    for (size_t i = 0; i < _dlhandler_off; i++) {
        if (_dlhandler_vec[i] == handler) {
            memmove(_dlhandler_vec + i, _dlhandler_vec + i + 1, (_dlhandler_off - i - 1) * sizeof(*handler));
            _dlhandler_off--;
            return;
        }
    }
}
