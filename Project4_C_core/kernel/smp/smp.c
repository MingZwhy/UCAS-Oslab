#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

#include <asm.h>

//global core_lock
//0 --> UNLOCKED ; 1 --> LOCKED
uint32_t core_lock;

uint64_t pidn_addr = (uint64_t)&pidn_pcb;
#define TIMER_INTERVAL 50000

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    disable_interrupt();

    printl("before send_ipi\n");
    send_ipi(0);
    printl("after send_ipi\n");

    clear_sip();
    enable_interrupt();
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    
    /*
    int id = get_current_cpu_id();

    if(id == 0)
        printl("main core want to get key\n");
    else
        printl("n core want to get key\n");
    */
    
    while (atomic_swap(1, &core_lock) == 1);

    /*
    if(id==0)
    {
        printl("main acquire successfully!\n");
    }
    else
    {
        printl("next acquire successfully!\n");
    } 
    */   
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    
    core_lock = 0;

    /*
    int id = get_current_cpu_id();
    if(id == 0)
        printl("main core release core_lock!\n");
    else
        printl("next core release core_lock!\n");
    */
}
