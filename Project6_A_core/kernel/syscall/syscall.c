#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    /*register unsigned long a7 asm ("a7");
    uint64_t num = a7;
    register unsigned long a0 asm ("a0") = (unsigned long)(regs->regs[10]);   
    register unsigned long a1 asm ("a1") = (unsigned long)(regs->regs[11]);   
    register unsigned long a2 asm ("a2") = (unsigned long)(regs->regs[12]); 
    register unsigned long a3 asm ("a3") = (unsigned long)(regs->regs[13]); 
    register unsigned long a4 asm ("a4") = (unsigned long)(regs->regs[14]); 
    regs->regs[10]=(reg_t)syscall[num]();
    regs->sepc = (regs->sepc) + 4;*/
    
    //to satisfy O2
    reg_t sys_id= (reg_t)regs->regs[17];
    reg_t para0 = (reg_t)regs->regs[10];
    reg_t para1 = (reg_t)regs->regs[11];
    reg_t para2 = (reg_t)regs->regs[12];
    reg_t para3 = (reg_t)regs->regs[13];
    reg_t para4 = (reg_t)regs->regs[14];
    regs->regs[10] = (reg_t)syscall[sys_id](para0, para1, para2, para3, para4);

    regs->sepc = (regs->sepc) + 4;
}
