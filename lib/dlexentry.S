#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

# Page fault upcall entrypoint.

# This is where we ask the kernel to redirect us to whenever we cause
# a page fault in user space (see the call to sys_set_pgfault_handler
# in pgfault.c).
#
# When a page fault actually occurs, the kernel switches our RSP to
# point to the user exception stack if we're not already on the user
# exception stack, and then it pushes a UTrapframe onto our user
# exception stack:
#
#  utf_rsp
#  utf_rflags
#  utf_rip
#  utf_regs.reg_rax
#  ...
#  utf_regs.reg_r15
#  utf_err (error code)
#  utf_fault_va <-- %rsp
#
# If this is a recursive fault, the kernel will reserve for us a
# blank word above the trap-time rsp for scratch work when we unwind
# the recursive call.
#
# We then have call up to the appropriate page fault handler in C
# code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _dlex_upcall
_dlex_upcall:
    # Call the C page fault handler.
    movq  %rsp,%rdi # passing the function argument in rdi
    movabs $_handle_deadline_exeeded, %rax
    call *%rax

    addq $16, %rsp # ignore fault_va and error code
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rdx
    popq %rcx

    movq 32(%rsp), %rbx
    movq 16(%rsp), %rax
    subq $8, %rbx
    movq %rax, (%rbx)
    movq %rbx, 32(%rsp)

    popq %rbx
    popq %rax

    # Restore rflags from the stack.  After you do this, you can
    # no longer use arithmetic operations or anything else that
    # modifies rflags.
    # LAB 9: Your code here

    pushq 8(%rsp)
    popfq

    # Switch back to the adjusted trap-time stack.
    # LAB 9: Your code here
    
    movq 16(%rsp), %rsp
    # Return to re-execute the instruction that faulted.
    ret
