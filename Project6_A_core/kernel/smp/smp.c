#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>
uint32_t core_lock;//0 is UNLOCKED,1 is LOCKED
void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
    // *(&core_lock) = 0;
    // setup_exception();

    // uint64_t pid0_tp = (regs_context_t *)(pid0_pcb.kernel_sp + sizeof(switchto_context_t));

    // asm volatile(
    //     "nop\n"
    //     "mv tp, %0\n"
    //     :
    //     : "r"(pid0_tp));

    // enable_preempt();
    // bios_set_timer(get_timer() + 100000);
    // asm volatile("wfi");
    // return;
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    disable_interrupt();
    send_ipi(0);
    disable_sip();
    enable_interrupt();
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    while (atomic_swap(1, pa2kva(&core_lock))==1);
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    core_lock = 0;
}
