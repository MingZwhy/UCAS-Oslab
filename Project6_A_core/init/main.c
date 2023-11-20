/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/ioremap.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/net.h>
#include <os/fs.h>
extern void ret_from_exception();

#define TASKNUM_LOC 0x502001fa
#define TASKINFO_LOC 0X502001f8
#define A_core 1
task_info_t tasks[TASK_MAXNUM];

int task_num = 0;
int task_info = 0;
static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR] = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ] = (long (*)())sd_read;
    jmptab[SD_WRITE] = (long (*)())sd_write;
    jmptab[QEMU_LOGGING] = (long (*)())qemu_logging;
    jmptab[SET_TIMER] = (long (*)())set_timer;
    jmptab[READ_FDT] = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR] = (long (*)())screen_move_cursor;
    jmptab[PRINT] = (long (*)())printk;
    jmptab[YIELD] = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT] = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ] = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE] = (long (*)())do_mutex_lock_release;
}

static void clear_121()
{
    PTE *pgdir = (PTE *)(MAIN_DIR);
    for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu;
         pa += 0x200000lu)
    {
        uint64_t va = pa;
        va &= VA_MASK;
        uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        if (pgdir[vpn2] != 0)
        {
            // alloc a new second-level page directory
            set_attribute(&pgdir[vpn2], 0);
        }
    }
}

static void init_task_info(void)
{
    short *tasknum = TASKNUM_LOC;
    task_num = *tasknum + 1;
    short *taskinfo = TASKINFO_LOC;
    task_info = *taskinfo;
    uint64_t real_info_loc = 0xffffffc051a00000;
    task_info_t *src = real_info_loc;
    memcpy(&tasks, src, 512);
    // for (int i = 1; i <= *tasknum; i++)
    // {
    //     memcpy(tasks[i].name, (char *)(0x502001f0 - i * 0x18), 16);
    //     memcpy(&tasks[i].start_block, (short *)(0x502001f0 - i * 0x18 + 0x10), 2);
    //     memcpy(&tasks[i].num_block, (short *)(0x502001f0 - i * 0x18 + 0x12), 2);
    //     memcpy(&tasks[i].offset, (int *)(0x502001f0 - i * 0x18 + 0x14), 4);
    // }
    clear_121();
    // for (int i = 1; i <= *tasknum; i++)
    // {
    //     load_task_img(i);
    // }
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, e  tc.).
     */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++)
    {
        pt_regs->regs[i] = 0;
    }
    pt_regs->regs[4] = pcb;
    pt_regs->regs[2] = user_stack;
    pt_regs->regs[1] = exception_handler_entry;
    pt_regs->sstatus = SR_SPIE | SR_SUM;
    pt_regs->sepc = entry_point;
    // printl("the address of regcontext is at %x\n", entry_point);
    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    //  printl("The context address is at %x\n", pt_switchto);
    for (int i = 0; i < 14; i++)
    {
        pt_switchto->regs[i] = 0;
    }
    pt_switchto->regs[0] = &ret_from_exception; // ra寄存器的值

    //  printl("the address of ret point is at %x\n", &(pt_switchto->regs[0]));
    // printl("the value of the ret point is %x\n", pt_switchto->regs[0]);

    pcb->kernel_sp = pt_switchto;
    pt_switchto->regs[1] = pcb->kernel_sp;
    //  printl("the latest kernel sp is at %x\n", pcb->kernel_sp);
    //  printl("\n");
}

static void init_pcb(char *name)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        pcb[i].status = TASK_EMPTY;
    }
    int taskidx = -1;
    for (int i = 0; i < task_num; i++)
    {
        if (strcmp(tasks[i].name, name) == 0)
        {
            taskidx = i;
            break;
        }
    }
    if (taskidx == -1)
    {
        printl("~~There is no %s in tasks~~", name);
        return;
    }
    int numpage = 1;
    pcb[0].pid = 0;
    pcb[0].pgdir = allocFixedPage(numpage, MAIN_DIR);
    share_pgtable(pcb[0].pgdir, MAIN_DIR);
    pcb[0].wakeup_time = 0;
    pcb[0].status = TASK_READY;
    pcb[0].cursor_x = 0;
    pcb[0].cursor_y = 0;
    alloc_page_helper(USER_SP, pcb[0].pgdir, 0, 0);
    ptr_t kernel_sp = allocFixedPage(numpage, MAIN_DIR);
    ptr_t user_sp = USER_SP;
    pcb[0].kernel_stack_base = kernel_sp;
    pcb[0].user_stack_base = user_sp;
    pcb[0].kernel_sp = pcb[0].kernel_stack_base + numpage * PAGE_SIZE;
    pcb[0].user_sp = pcb[0].user_stack_base + numpage * PAGE_SIZE;
    pcb[0].core = 0;
    pcb[0].mask = 1;
    pcb[0].wait_list.next = &(pcb[0].wait_list);
    pcb[0].wait_list.prev = &(pcb[0].wait_list);
    en_queue(&ready_queue, &pcb[0].list);
    uint64_t address = load_task_img(taskidx, 0);
    // int address = load_address(taskidx);
    init_pcb_stack(pcb[0].kernel_sp, pcb[0].user_sp, address, &pcb[0]);
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running0 = &pid0_pcb;
    current_running = current_running0;
    pid0_pcb.list.next = &pcb[0].list;
    return;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_EXEC] = (long (*)())do_exec;
    syscall[SYSCALL_EXIT] = (long (*)())do_exit;
    syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
    syscall[SYSCALL_KILL] = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_PS] = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE] = (long (*)())screen_write;
    syscall[SYSCALL_READCH] = (long (*)())port_read_ch;
    syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT] = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT] = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY] = (long (*)())do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;
    syscall[SYSCALL_SHM_GET] = (long (*)())do_shmpageget;
    syscall[SYSCALL_SHM_DT] = (long (*)())do_shmpagedt;

    syscall[SYSCALL_THREAD_CREATE] = (long (*)())thread_create;
    syscall[SYSCALL_TASK_SET] = (long (*)())do_taskset;
    syscall[SYSCALL_SRCH_PA] = (long (*)())get_page_addr;
    syscall[SYSCALL_SNAP_GET] = (long (*)())do_snapget;

    syscall[SYSCALL_NET_SEND]           = (void *)do_net_send;
    syscall[SYSCALL_NET_RECV]           = (void *)do_net_recv;

    syscall[SYSCALL_FS_MKFS] = (long (*)())do_mkfs;
    syscall[SYSCALL_FS_LS] = (long (*)())do_ls;
    syscall[SYSCALL_FS_MKDIR] = (long (*)())do_mkdir;
    syscall[SYSCALL_FS_RMDIR] = (long (*)())do_rmdir;
    syscall[SYSCALL_FS_CD] = (long (*)())do_cd;
    syscall[SYSCALL_FS_STATFS] = (long (*)())do_statfs;

    syscall[SYSCALL_FS_TOUCH] = (long (*)())do_touch;
    syscall[SYSCALL_FS_CAT] = (long (*)())do_cat;
    syscall[SYSCALL_FS_FOPEN] = (long (*)())do_fopen;
    syscall[SYSCALL_FS_FREAD] = (long (*)())do_fread;
    syscall[SYSCALL_FS_FWRITE] = (long (*)())do_fwrite;
    syscall[SYSCALL_FS_FCLOSE] = (long (*)())do_fclose;

    syscall[SYSCALL_FS_LN] = (long (*)())do_ln;
    syscall[SYSCALL_FS_RM] = (long (*)())do_rm;
    syscall[SYSCALL_FS_LSEEK] = (long (*)())do_lseek;
    syscall[SYSCALL_FS_CLEARSD] = (void *)do_clearSD;
}

int main(void)
{
    uint64_t ipi = get_current_cpu_id();
    if (ipi == 0)
    {
        current_running = &pid0_pcb;
        current_running0 = &pid0_pcb;
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        // Init Process Control Blocks |•'-'•) ✧
        init_shmpage();

        // Read Flatten Device Tree (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        e1000 = (volatile uint8_t *)bios_read_fdt(EHTERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: 0x%lx, plic_addr: 0x%lx, nr_irqs: 0x%lx.\n", e1000, plic_addr, nr_irqs);

        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");
        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");
        
        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");
        // TODO: [p5-task4] Init plic
        plic_init(plic_addr, nr_irqs);
        printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);
        init_pcb("shell");

        // Init file system
        do_mkfs();
        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");
        
        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        lock_kernel();
        // wakeup_other_hart();
        enable_preempt();
        //enable_interrupt();
    }
    else
    {
        lock_kernel();
        setup_exception();
        current_running1 = &pid1_pcb;
        enable_preempt();
    }
    while (1)
    {
        do_scheduler();
        unlock_kernel();
        lock_kernel();
    }
    return 0;
}
