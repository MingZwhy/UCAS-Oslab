#include <sys/syscall.h>
#include <type.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */

    /*
    register unsigned long a0 asm ("a0") = (unsigned long)(arg0);
    register unsigned long a1 asm ("a1") = (unsigned long)(arg1);
    register unsigned long a2 asm ("a2") = (unsigned long)(arg2);
    register unsigned long a3 asm ("a3") = (unsigned long)(arg3);
    register unsigned long a4 asm ("a4") = (unsigned long)(arg4);
    register unsigned long a7 asm ("a7") = (unsigned long)(sysno);
    */

    reg_t syscall_num = regs->regs[17];
    reg_t param0 = regs->regs[10];
    reg_t param1 = regs->regs[11];
    reg_t param2 = regs->regs[12];
    reg_t param3 = regs->regs[13];
    reg_t param4 = regs->regs[14];

    reg_t return_value;
    return_value = syscall[syscall_num](param0, param1, param2, param3, param4);

    //store return_value into a0
    regs->regs[10] = (reg_t)return_value;

    //sepc = sepc + 4
    regs->regs[33] = regs->regs[33] + 4;
}
