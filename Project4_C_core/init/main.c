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
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <pgtable.h>

#include <os/smp.h>

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];

void init_pid_valid(void)
{
    pid_valid[0] = 1;      //needn't recovery shell
    for(int i=1; i<NUM_MAX_TASK; i++)
    {
        pid_valid[i] = 0;
    }
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;
}

static void get_loc_and_num(short *task_info_loc, short *tasknum)
{
    uint64_t addr1 = 0xffffffc0502001fa;
    short * src1 = addr1;
    memcpy(task_info_loc, src1, sizeof(short));

    uint64_t addr2 = 0xffffffc0502001f4;
    short * src2 = addr2;
    memcpy(tasknum, src2, sizeof(short));
    //printk("loc is %d, tasknum is %d\n", *task_info_loc, *tasknum);
    //printl("loc is %d, tasknum is %d\n", *task_info_loc, *tasknum);
}

static void init_task_info(short task_info_loc, short tasknum)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock 

    //address of image info in memory
    //int real_info_loc = 0x52030000 + task_info_loc;
    //in task4, it should be virtual address
    uint64_t real_info_loc = 0xffffffc054000000 + task_info_loc;
    //memcopy image info 
    task_info_t * src = real_info_loc;
    memcpy(tasks, src, sizeof(task_info_t)*tasknum);
}

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    printl("regs address is %x\n", pt_regs);

    pcb->user_sp = user_stack;

    //store address of ret_from_exception into ra reg in regs_context_t
    pt_regs->regs[1] = &ret_from_exception;  
    //store ptr to user_stack into sp reg in regs_context_t
    pt_regs->regs[2] = user_stack;
    pt_regs->regs[4] = pcb;
    /*
    store entry_point of app into sepc reg in regs_context_to
    so that we can get into entry of app in path:
    switch_to --> ret_from_exception --> app
    first time in ret_from_exception:
    step1: RESTORE_CONTEXT (In RESTORE_CONTENT, We load entry_point into sepc)
    step2: sret (sepc --> pc)

    after the first time: we continue to run the app in path:
    SAVE_CONTEXT(In exception_handler_entry) --> exception --> RESTORE_CONTEXT(In ret_from_exception) --> app
    in handle_syscall , we make sepc_reg in regs_context_t += 4 so that we can
    continue to run app in right position
    */              
    pt_regs->sepc = entry_point;
    /*
    because when we first set CSR_SSTATUS, we are in kernel
    (when we begin first pthread through switch_to --> ret_from_exception)
    we RESTORE_CONTEXT in ret_from_exception
    in RESTORE_CONTEXT, we set CSR_SSTATUS (from pt_regs->sstatus)
    after that, "sret" make us return to User-mode (SR_SIE)
    so
    in kernel <--> SR_SPIE
    so we store SR_SPIE into sstatus_reg in init_pcb_stack
    */
    pt_regs->sstatus = (SR_SPIE & ~SR_SPP) | SR_SUM;

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */

    /* used to save register infomation in switch_to
    typedef struct switchto_context
    {
        //Callee saved registers.
        reg_t regs[14];
    } switchto_context_t;
    */

    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    //令sp_of_kernel指向pt_switchto的底部
    pcb->kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
    printl("kernel_sp is %x\n",pt_switchto);
    
    /*
    task1-2:
    according to offset in switch_to, regs[0] store SWITCH_TO_RA --> entry_point
    pt_switchto->regs[0] = entry_point;
    */

    /*task3-5
    not like in task1-2 (in the end of switch_to, we jump to next app directly)
    in task3-5, we need to RESTORE_CONTEXT and return to User-mode through ret_from_exception
    in the first round scheduling 
    我们在进入第一个程序时，是从特殊的pid0直接switch_to过去的，也就是说它实际上没有经历后序正常的
    sys_yield() --> invoke_syscall() --> exception_handler_entry() --> interrupt_helper -->
    handle_syscall() --> do_schedler() --> switch_to() --> ret_from_exception --> next_app
    他是直接do_schedler() --> switch_to()，
    也就是说它在switch_to的末位无法正常回到exception_handler_entry()中call interrupt的下一条指令
    (la ra, ret_from_exception + jr ra)，再跳到ret_from_exception
    所以我们需要一些特殊的设定让他直接从switch_to末尾跳到ret_from_exception,

    而这个特殊的设定就是pt_switchto->regs[0] = &ret_from_exception;
    因此我们在switch_to下半段的ld ra, SWITCH_TO_RA(t0)后将ra设为了ret_from_exception的地址

    而对第一轮调度的其他程序(除了第一个运行的程序)，虽然他有从上一个程序
    sys_yield() --> invoke_syscall() --> exception_handler_entry() --> interrupt_helper -->
    handle_syscall()的过程，单由于它从来没有调用sys_yield()过，因此他从从来向他的switch_context
    中存过东西，因此他没法从pt_switchto->regs[0]中load出exception_handler_entry()中call interrupt的下一条指令
    的地址，也就没法回到ret_from_exception，因此他也需要pt_switchto->regs[0] = &ret_from_exception

    因此由于两个必要条件的限定，第一轮调度每一个程序的需要pt_switchto->regs[0] = &ret_from_exception

    除了第一轮调度外，都有sys_yield() --> invoke_syscall() --> exception_handler_entry() --> interrupt_helper -->
    的正常过程，同时每一个程序也都在switch_to中向其context中存过一次ra寄存器，
    所以能在switch_to的末尾回到exception_handler_entry()中call interrupt的下一条指令
    凭借(la ra, ret_from_exception + jr ra)也能正常回到
    ret_from_exception
    */
    pt_switchto->regs[0] = &ret_from_exception;

    printl("store entry_point %x in RA reg\n", pt_switchto->regs[0]);
    printl("actual entry_point is %x\n",entry_point);
    //according to offset in switch_to, regs[1] store SWITCH_TO_SP --> sp_of_kernel
    pt_switchto->regs[1] = pcb->kernel_sp;
    printl("store kernel_sp %x in SP reg\n\n", pt_switchto->regs[1]);
}

static void init_pcb(short tasknum, char *name, int pid)
{
    ptr_t bottom1, bottom2, roof1, roof2;
    for(int i=0; i<tasknum; i++)
    {
        printl("task name is %s\n",tasks[i].file_name);
        if(strcmp(tasks[i].file_name, name) == 0)
        {
            //初始化进程的页表地址数组
            pcb[i].num_of_page = 0;
            for(int j=0; j < 128; j++)
            {
                pcb[i].page_addr[j] = 0;
            }

            //printl("finish page_addr init\n");

            pcb[i].pgdir = allocPgdir(1,&pcb[i]);
            //注意这里两个参数都要是虚拟地址
            share_pgtable(pcb[i].pgdir, pa2kva(PGDIR_PA));

            //printl("finish share_pgtable\n");

            //为内核栈分配一页
            pcb[i].kernel_stack_base = allocKernPage(1, &pcb[i]);
            //pcb[i].kernel_stack_base = allocPgdir(1, &pcb[i]);
            //printl("kernel_stack_base is %x\n",pcb[i].kernel_stack_base);
            //printl("num_of_page is%d\n", pcb[i].num_of_page);
            uint64_t roof1 = pcb[i].kernel_stack_base + PAGE_SIZE;

            //为用户栈分配一页
            pcb[i].user_stack_base = USER_STACK_ADDR;
            uint64_t roof2 = pcb[i].user_stack_base + PAGE_SIZE;

            printl("-------------------\n");

            uint64_t user_address = alloc_page_helper(USER_STACK_ADDR, pcb[i].pgdir, &pcb[i], TRUE);
            //printl("user address is %x\n", user_address);

            //printl("finish alloc_page_helper\n");

            //Load the task, return the app_address
            uint64_t app_address = load_task_img(i, pcb[i].pgdir, &pcb[i]);
            //printl("app_address is %x\n",app_address);
            //init the task_name
            strcpy(pcb[i].name, tasks[i].file_name);

            init_pcb_stack(roof1, roof2, app_address, &pcb[i]);

            //分配pid
            pcb[i].pid = pid;
            pcb[i].pcb_valid = 1;

            for(int j=0; j<10; j++)
            {
                pcb[i].hold_lock_id[j] = 0;
            }

            pcb[i].mask = 1;
            pcb[i].core_id = get_current_cpu_id();

            //初始化cursor
            pcb[i].cursor_x = 0;
            pcb[i].cursor_y = 0;

            //初始化状态
            pcb[i].status = TASK_READY;

            //初始化唤醒时间
            pcb[i].wakeup_time = 0;

            //将任务入队列
            //void Enqueue(list_head * l, list_node * new_node)
            Enqueue(&ready_queue, &pcb[i].list);
            Print_queue(&ready_queue);
        }
    }
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_EXEC]               = (void *)do_exec;
    syscall[SYSCALL_EXIT]               = (void *)do_exit;
    syscall[SYSCALL_SLEEP]              = (void *)do_sleep;
    syscall[SYSCALL_KILL]               = (void *)do_kill;
    syscall[SYSCALL_WAITPID]            = (void *)do_waitpid;
    syscall[SYSCALL_PS]                 = (void *)do_process_show;
    syscall[SYSCALL_GETPID]             = (void *)do_getpid;
    syscall[SYSCALL_YIELD]              = (void *)do_scheduler;
    syscall[SYSCALL_WRITE]              = (void *)screen_write;
    syscall[SYSCALL_READCH]             = (void *)bios_getchar;
    syscall[SYSCALL_CURSOR]             = (void *)screen_move_cursor;
    syscall[SYSCALL_REFLUSH]            = (void *)screen_reflush;
    syscall[SYSCALL_CLEAR]              = (void *)screen_clear;
    syscall[SYSCALL_GET_TIMEBASE]       = (void *)get_time_base;
    syscall[SYSCALL_GET_TICK]           = (void *)get_ticks;
    syscall[SYSCALL_LOCK_INIT]          = (void *)do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]           = (void *)do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]       = (void *)do_mutex_lock_release;
    syscall[SYSCALL_SHOW_TASK]          = (void *)NULL;
    syscall[SYSCALL_BARR_INIT]          = (void *)do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]          = (void *)do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]       = (void *)do_barrier_destroy;
    syscall[SYSCALL_COND_INIT]          = (void *)do_condition_init;
    syscall[SYSCALL_COND_WAIT]          = (void *)do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]        = (void *)do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST]     = (void *)do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]       = (void *)do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN]          = (void *)do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]         = (void *)do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]          = (void *)do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]          = (void *)do_mbox_recv;

    //task5 --> thread_create
    syscall[SYSCALL_THREAD_CREATE]      = (void *)thread_create;
    syscall[SYSCALL_TASKSET_EXEC]       = (void *)taskset_exec;
    syscall[SYSCALL_TASKSET]            = (void *)taskset;
    
    //prj4 task5
    syscall[SYSCALL_SHM_GET]            = (void *)shm_page_get;
    syscall[SYSCALL_SHM_DT]             = (void *)shm_page_dt;
    syscall[SYSCALL_F]                  = (void *)get_page_addr;
    syscall[SYSCALL_SNP_GET]            = (void *)snp_page_get;
}

int main()
{
    short task_info_loc;
    short tasknum;
    get_loc_and_num(&task_info_loc, &tasknum);
    int id = get_current_cpu_id();
    if(id == 0)        //主核
    {
        //clear_temporary_map();
        init_mm();
        init_jmptab();
        init_task_info(task_info_loc, tasknum);
        printl("will init pcb, loc is %d,tasknum is %d\n",task_info_loc, tasknum);
        init_pcb(tasknum, "shell", 0);

        //while(1);

        current_running = &pid0_pcb;
        current_running_m = &pid0_pcb;

        time_base = bios_read_fdt(TIMEBASE);
        
        //while(1);

        init_locks();
        init_exception();
        init_syscall();
        init_screen();

        init_pid_valid();

        //wake up anthor core
        lock_kernel();
        wakeup_other_hart();

        enable_preempt();
    }
    else              //从核
    {
        lock_kernel();

        setup_exception();
        current_running_n = &pidn_pcb;
        enable_preempt();
    }


    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {     
        do_scheduler();
        unlock_kernel();
        lock_kernel();
    }

    return 0;
}

