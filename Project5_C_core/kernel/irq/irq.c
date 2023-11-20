#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <csr.h>
#include <e1000.h>
#include <plic.h>

#include <os/smp.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    //printl("in helper\n");
    lock_kernel();
    int id = get_current_cpu_id();
    if(id == 0)
    {
        //printl("id = 0, cur = m\n");
        current_running = current_running_m;
    }
    else
    {
        //printl("id = 1, cur = n\n");
        current_running = current_running_n;
    }
    
    uint64_t exc_or_irq     = SCAUSE_IRQ_FLAG & scause;
    uint64_t Exception_code = ~SCAUSE_IRQ_FLAG & scause;   

    if(exc_or_irq)
    {
        //SCAUSE[63] == 1: Interrupt
        handler_t handle_func = irq_table[Exception_code];
        handle_func(regs, stval, scause);
    }
    else
    {
        //SCAUSE[63] == 0: Syscall
        handler_t handle_func = exc_table[Exception_code];
        handle_func(regs, stval, scause);
    }

}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    //prj5:check if send and recv blocked can be released
    // check_net_send();
    // check_net_recv();

    //step1：reset the timer
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    //step2：reschedule
    do_scheduler();
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task4] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
    int hwirq = plic_claim();
    if (hwirq !=PLIC_E1000_PYNQ_IRQ)
    {
        plic_complete(hwirq);
        return;
    }
    uint32_t ecode = e1000_read_reg(e1000, E1000_ICR);
    uint32_t IMS_MASK = e1000_read_reg(e1000, E1000_IMS);
    ecode = ecode & IMS_MASK;

    int if_txqe_ex = ecode ^ E1000_ICR_TXQE;
    int if_rxdmt0_ex = ecode ^ E1000_ICR_RXDMT0;
    
    if(if_txqe_ex==0)
    {
        e1000_handle_txqe();
    }

    if(if_rxdmt0_ex==0)
    {
        e1000_handle_rxdmt0();
    }

    plic_complete(hwirq);
}

void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    do_pagefault(stval);
    return;
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/

    /*
    enum ExcCode
    {
        EXCC_INST_MISALIGNED  = 0,
        EXCC_INST_ACCESS      = 1,
        EXCC_BREAKPOINT       = 3,
        EXCC_LOAD_ACCESS      = 5,
        EXCC_STORE_ACCESS     = 7,
        EXCC_SYSCALL          = 8,
        EXCC_INST_PAGE_FAULT  = 12,
        EXCC_LOAD_PAGE_FAULT  = 13,
        EXCC_STORE_PAGE_FAULT = 15,
        EXCC_COUNT
    };
    */
    for(int i=0;i<16;i++)
    {
        exc_table[i] = (void *)handle_other;
    }
    exc_table[EXCC_SYSCALL]              = (void *)handle_syscall;   //environment call from U-mode
    exc_table[EXCC_INST_MISALIGNED]      = (void *)handle_other;
    exc_table[EXCC_INST_ACCESS]          = (void *)handle_other;
    exc_table[EXCC_BREAKPOINT]           = (void *)handle_other;
    exc_table[EXCC_LOAD_ACCESS]          = (void *)handle_other;
    exc_table[EXCC_STORE_ACCESS]         = (void *)handle_page_fault;
    exc_table[EXCC_INST_PAGE_FAULT]      = (void *)handle_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT]      = (void *)handle_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT]     = (void *)handle_page_fault;
    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    /*
    enum IrqCode
    {
        IRQC_U_SOFT  = 0,
        IRQC_S_SOFT  = 1,
        IRQC_M_SOFT  = 3,
        IRQC_U_TIMER = 4,
        IRQC_S_TIMER = 5,
        IRQC_M_TIMER = 7,
        IRQC_U_EXT   = 8,
        IRQC_S_EXT   = 9,
        IRQC_M_EXT   = 11,
        IRQC_COUNT
    };
    */

    for(int i=0;i<12;i++)
    {
        irq_table[i] = (void *)handle_other;
    }
    irq_table[IRQC_S_TIMER]               = (void *)handle_irq_timer;
    irq_table[IRQC_U_SOFT]                = (void *)handle_other;
    irq_table[IRQC_S_SOFT]                = (void *)handle_other;
    irq_table[IRQC_M_SOFT]                = (void *)handle_other;
    irq_table[IRQC_U_TIMER]               = (void *)handle_other;
    irq_table[IRQC_M_TIMER]               = (void *)handle_other;
    irq_table[IRQC_U_EXT]                 = (void *)handle_other;
    irq_table[IRQC_S_EXT]                 = (void *)handle_irq_ext;
    irq_table[IRQC_M_EXT]                 = (void *)handle_other;

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    //while(1);
    setup_exception();
    //printl("end setup_exception\n");
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
