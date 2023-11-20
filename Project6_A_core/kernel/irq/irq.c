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

#define SCAUSE_IRQ_FLAG (1UL << 63)

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{

    uint64_t ipi = get_current_cpu_id();
    if (ipi == 0)
    {
        current_running = current_running0;
    }
    else
    {
        current_running = current_running1;
    }
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    uint64_t is_irq = SCAUSE_IRQ_FLAG & scause;
    uint64_t True_cause = ~SCAUSE_IRQ_FLAG & scause;
    // if (True_cause > 16)
    // {
    //     assert(0);
    // }
    if (is_irq)
    {
        irq_table[True_cause](regs, stval, scause);
    }
    else
    {
        exc_table[True_cause](regs, stval, scause);
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    // check_net_send();
    // check_net_recv();

    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();
    return;
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task4] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
    int hwirq = plic_claim();
    // printk("hwirq is %d\n",hwirq);
    if (hwirq != PLIC_E1000_PYNQ_IRQ)
    {
        plic_complete(hwirq);
        return;
    }
    uint32_t ecode = e1000_read_reg(e1000, E1000_ICR);
    uint32_t IMS = e1000_read_reg(e1000, E1000_IMS);
    ecode = ecode & IMS;
    //printk("ecode is %d\n",ecode);
    int if_txqe = ecode ^ E1000_ICR_TXQE;
    int if_rxdmt0 = ecode ^ E1000_ICR_RXDMT0;
    // printk("if_rxdmt0 is %d\n",if_rxdmt0);
    if (if_txqe == 0)
    {
        e1000_handle_txqe();
    }
    if (if_rxdmt0 == 0)
    {
        // int tx_head = e1000_read_reg(e1000, E1000_RDH);
        // int tx_tail = e1000_read_reg(e1000, E1000_RDT);
        // printk("head is %d\n",tx_head);
        // printk("tail is %d\n",tx_tail);
        e1000_handle_rxdmt0();
    }
    // else{
    //     assert(0);
    // }
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
    for (int i = 0; i < EXCC_COUNT; i++)
    {
        if (i == EXCC_SYSCALL)
        {
            exc_table[i] = handle_syscall;
        }
        else if (i == EXCC_STORE_PAGE_FAULT || EXCC_INST_PAGE_FAULT)
        {
            exc_table[i] = handle_page_fault;
        }
        else
        {
            exc_table[i] = handle_other;
        }
    }
    for (int i = 0; i < IRQC_COUNT; i++)
    {
        if (i == IRQC_S_TIMER)
        {
            irq_table[i] = handle_irq_timer;
        }
        else if (i == IRQC_S_EXT)
        {
            irq_table[i] = handle_irq_ext;
        }
        else
        {
            irq_table[i] = handle_other;
        }
    }
    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
    return;
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char *reg_name[] = {
        "zero ", " ra  ", " sp  ", " gp  ", " tp  ",
        " t0  ", " t1  ", " t2  ", "s0/fp", " s1  ",
        " a0  ", " a1  ", " a2  ", " a3  ", " a4  ",
        " a5  ", " a6  ", " a7  ", " s2  ", " s3  ",
        " s4  ", " s5  ", " s6  ", " s7  ", " s8  ",
        " s9  ", " s10 ", " s11 ", " t3  ", " t4  ",
        " t5  ", " t6  "};
    for (int i = 0; i < 32; i += 3)
    {
        for (int j = 0; j < 3 && i + j < 32; ++j)
        {
            printk("%s : %016lx ", reg_name[i + j], regs->regs[i + j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
