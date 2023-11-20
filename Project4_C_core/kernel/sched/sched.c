#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <csr.h>
#include <os/kernel.h>

#include <os/task.h>    //for task_intfo_t tasks
#include <pgtable.h>

#define bool int
#define true 1
#define false 0

page pages_mm[MAX_STU_PAGE_NUM];

int if_first_do_sche = 1;
const ptr_t kernel_pgdir = PGDIR_PA + KVA_PA_OFFSET;

pcb_t pcb[NUM_MAX_TASK];
pcb_t tcb[NUM_MAX_THREAD];
int tcb_index = 0;

//using in thread_create
extern void ret_from_exception();

typedef struct argv_addrs
{
    uint64_t argv_addr[6];
} argv_addrs, *ptr_to_argv_addrs;

const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .pgdir = (ptr_t)kernel_pgdir,
    .name = "empty_m"
};

const ptr_t pidn_stack = NEXT_KERNEL_STACK + PAGE_SIZE;
pcb_t pidn_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pidn_stack,
    .user_sp = (ptr_t)pidn_stack,
    .pgdir = (ptr_t)kernel_pgdir,
    .name = "empty_n"
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;
pcb_t * volatile current_running_m;  //主核
pcb_t * volatile current_running_n;  //从核

pid_t pid_valid[NUM_MAX_TASK];

//int pid_valid[NUM_MAX_TASK];     //pid_valid used for recovery pid
pid_t alloc_pid(void)
{
    for(int i=0; i<NUM_MAX_TASK; i++)
    {
        if(pid_valid[i] == 0)
        {
            pid_valid[i] = 1;
            return i;
        }
    }
    return 0;      //in my design, return 0 means no valid pid (shell's pid is 0)
}

void recover_pid(pid_t p)
{
    if(p > 0 && p < NUM_MAX_TASK)
    {
        pid_valid[p] = 0;
    }
    else
    {
        printk("wrong recover pid!\n");
    }
}

static ptr_t add_user_stack(ptr_t k_user_stack, ptr_t u_user_stack,
                            pcb_t *pcb, int argc, char *argv[])
{
    ptr_t k_user_sp = k_user_stack;
    ptr_t u_user_sp = u_user_stack;

    k_user_sp = k_user_sp - sizeof(uint64_t) * argc;
    u_user_sp = u_user_sp - sizeof(uint64_t) * argc;

    argv_addrs *ptr_Addrs = (ptr_to_argv_addrs)(k_user_sp);
    ptr_t argv_base = u_user_sp;

    for(int i=0; i<argc; i++)
    {
        int len = strlen(argv[i]);
        k_user_sp = k_user_sp - (sizeof(char) * (len+1));
        u_user_sp = u_user_sp - (sizeof(char) * (len+1));
        
        char * s = (char *)(k_user_sp);

        strcpy(s,argv[i]);
        //printl("\n\n\ns is %s\n\n\n",s);

        ptr_Addrs->argv_addr[i] = (ptr_t)u_user_sp;
        //ptr_Addrs->argv_addr[i] = s;
    }

    int align = (k_user_stack - k_user_sp) / 16;
    pcb->user_sp = u_user_stack - (align + 1) * 16;

    return argv_base;

}

void add_pcb_stack(
    ptr_t kernel_stack, ptr_t pt_argvs, ptr_t entry_point,
    pcb_t *pcb, int argc, bool if_thread)
{
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    pt_regs->regs[1] = &ret_from_exception; 
    pt_regs->regs[2] = pcb->user_sp;
    pt_regs->regs[4] = pcb;

    if(if_thread)
    {
        pt_regs->regs[10] = pt_argvs;
    }
    else
    {
        pt_regs->regs[10] = (uint64_t)argc;                     //argc --> a0
    }
    pt_regs->regs[11] = (uint64_t)pt_argvs;                 //argv_base --> a1


    pt_regs->sepc = entry_point;
    pt_regs->sstatus = SR_SPIE | SR_SUM | SR_FS;

    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    pt_switchto->regs[0] = &ret_from_exception;

    pcb->kernel_sp = pt_switchto;
    pt_switchto->regs[1] = pcb->kernel_sp;
}

pid_t add_pcb(char *name, int argc, char *argv[], int mask)
{
    ptr_t bottom1, bottom2, roof1, roof2;
    pid_t valid_pid;
    for(int i=0; i<NUM_MAX_TASK; i++)
    {
        if(strcmp(tasks[i].file_name, name) == 0)
        {
            int pcb_index;

            valid_pid = alloc_pid();
            pcb_index = valid_pid;

            pcb[pcb_index].pcb_valid = 1;
            strcpy(pcb[pcb_index].name, tasks[i].file_name);

            //must init wait_queue first
            init_queue(&pcb[pcb_index].wait_list);

            //init hold_lock
            for(int j=0; j<10; j++)
            {
                pcb[pcb_index].hold_lock_id[j] = 0;
            }

            pcb[pcb_index].mask = mask;
            pcb[pcb_index].core_id = get_current_cpu_id();

            //初始化进程的页表地址数组
            pcb[pcb_index].num_of_page = 0;
            for(int j=0; j < 128; j++)
            {
                pcb[pcb_index].page_addr[j] = 0;
            }

            for(int j=0; j < 128; j++)
            {
                pcb[pcb_index].kind[j] = 0;
            }

            pcb[pcb_index].pgdir = allocPgdir(1,&pcb[pcb_index]);
            printl("pgdir is %x\n\n", pcb[pcb_index].pgdir);
            share_pgtable(pcb[pcb_index].pgdir, pa2kva(PGDIR_PA));

            //为内核栈分配一页
            pcb[pcb_index].kernel_stack_base = allocKernPage(1, &pcb[pcb_index]);
            //pcb[pcb_index].kernel_stack_base = allocPgdir(1, &pcb[pcb_index]);
            uint64_t roof1 = pcb[pcb_index].kernel_stack_base + PAGE_SIZE;

            //为用户栈分配一页
            pcb[pcb_index].user_stack_base = USER_STACK_ADDR;
            uint64_t roof2 = pcb[pcb_index].user_stack_base + PAGE_SIZE;

            uintptr_t s_vaddress = alloc_page_helper(USER_STACK_ADDR, pcb[pcb_index].pgdir, &pcb[pcb_index], TRUE);
            uint64_t roofs = s_vaddress + PAGE_SIZE; 

            ptr_t pt_argvs = add_user_stack(roofs, roof2, &pcb[pcb_index], argc, argv);

            //Load the task, return the app_address
            uint64_t app_address = load_task_img(i, pcb[pcb_index].pgdir, &pcb[pcb_index]);

            add_pcb_stack(roof1, pt_argvs, app_address, &pcb[pcb_index], argc, false);

            pcb[pcb_index].pid = valid_pid;
            pcb[pcb_index].cursor_x = 0;
            pcb[pcb_index].cursor_y = 0;
            pcb[pcb_index].status = TASK_READY;
            pcb[pcb_index].wakeup_time = 0;
            Enqueue(&ready_queue, &pcb[pcb_index].list);
            //Print_queue(&ready_queue);

            return valid_pid;
        }
    }
    return 0;
}

void do_scheduler(void)
{
    //printl("into do_scheduler\n");
    uint64_t ipi = get_current_cpu_id();
    if (ipi == 0)
    {
        current_running = current_running_m;
    }
    else
    {
        current_running = current_running_n;
    }
    //printl("current_running is %s\n",current_running->name);

    check_sleeping();

    //printl("after checksleeping\n");

    pcb_t *prev_running;
    pcb_t *next_running;
    
    if (current_running->status == TASK_RUNNING)
    {
        current_running->status = TASK_READY;
    }

    prev_running = current_running;

    list_node_t *next_listnode = Dequeue(&ready_queue);

    if (next_listnode != NULL)
    {
        next_running = (pcb_t *)((char *)(next_listnode) - 2 * sizeof(reg_t) );

        if((next_running->mask != 3) && ((next_running->mask-1) != ipi))
        {
            printl("wrong mask, this core can't do %s\n",next_running->name);
            Enqueue(&ready_queue, next_listnode);
            next_running = (ipi==0) ? &pid0_pcb : &pidn_pcb;
        }
        else
        {
            
            if ((current_running == &pid0_pcb || current_running == &pidn_pcb) && if_first_do_sche)
            {
                set_timer(get_ticks() + TIMER_INTERVAL);
                if_first_do_sche = 0;
            }

            if (next_running->status == TASK_EXITED)
            {
                do_scheduler();
                return;
            }   
        }
    }
    else
    {
        
        if(current_running->status == TASK_RUNNING)
        {
            next_running = current_running;
            switch_to(prev_running,next_running);
        }
        else
        {
            if (ipi == 0)
                next_running = &pid0_pcb;
            else 
                next_running = &pidn_pcb;
        }
        
        /*
        if (ipi == 0)
            next_running = &pid0_pcb;
        else 
            next_running = &pidn_pcb;
        */
    }

    if ( (current_running != &pid0_pcb && current_running != &pidn_pcb) && (current_running->status == TASK_READY) )
    {
        Enqueue(&ready_queue, &prev_running->list);
    }

    next_running->status = TASK_RUNNING;
    current_running = next_running;

    if (ipi == 0)
    {
        current_running_m = next_running;
    }
    else
    {
        current_running_n = next_running;
    }
    next_running->core_id = ipi;

    //task4: switch asid and ppn
    set_satp(SATP_MODE_SV39, next_running->pid, kva2pa(next_running->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();

    //printl("next task is %s\n", next_running->name);

    switch_to(prev_running, next_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.

    current_running->status = TASK_BLOCKED;
    current_running->wakeup_time = get_timer() + sleep_time;
    Enqueue(&sleep_queue, &current_running->list);

    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    //do_block(&current_running->list, &mlocks[mlock_idx].block_queue);
    //step1: find ptr to cur_pcb
    pcb_t * cur_pcb = (pcb_t *)( (ptr_t)pcb_node - 2 * sizeof(reg_t) );
    //step2: change its state
    cur_pcb->status = TASK_BLOCKED;
    //step3: enqueue (block_queue for this lock)
    Enqueue(queue, pcb_node);


    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    //do_unblock(&mlocks[mlock_idx].block_queue);
    list_node_t * node = Dequeue(pcb_node);
    while(node != NULL)
    {
        pcb_t * cur_pcb = (pcb_t *)( (ptr_t)node - 2 * sizeof(reg_t) );
        if(cur_pcb->status == TASK_BLOCKED)
        {
            cur_pcb->status = TASK_READY;
            Enqueue(&ready_queue,node);
        }
        node = Dequeue(pcb_node);
    }
}

void thread_create(pid_t *thread, ptr_t func, uint64_t argv)
{
    pid_t valid_pid;
    int pcb_index;
    
    valid_pid = alloc_pid();
    *thread = valid_pid;
    pcb_index = valid_pid;
    pcb[pcb_index].pcb_valid = 1;
    //strcpy(pcb[pcb_index].name, "thread");

    //must init wait_queue first
    init_queue(&pcb[pcb_index].wait_list);

    //init hold_lock
    for(int j=0; j<10; j++)
    {
        pcb[pcb_index].hold_lock_id[j] = 0;
    }

    pcb[pcb_index].mask = current_running->mask;
    pcb[pcb_index].core_id = get_current_cpu_id();

    //初始化进程的页表地址数组
    pcb[pcb_index].num_of_page = 1;

    /*
    pcb[pcb_index].pgdir = allocPage(1,&pcb[pcb_index]);
    clear_pgdir(pcb[pcb_index].pgdir);
    share_pgtable(pcb[pcb_index].pgdir, pa2kva(PGDIR_PA));
    */

    //heritage pgdir from parent
    pcb[pcb_index].pgdir = current_running->pgdir;

    //为内核栈分配一页
    pcb[pcb_index].kernel_stack_base = allocKernPage(1, &pcb[pcb_index]);
    uint64_t roof1 = pcb[pcb_index].kernel_stack_base + PAGE_SIZE;

    //为用户栈分配一页
    pcb[pcb_index].user_stack_base = USER_STACK_ADDR_FOR_PTHREAD;
    uint64_t roof2 = pcb[pcb_index].user_stack_base + PAGE_SIZE;

    uintptr_t s_vaddress = alloc_page_helper(USER_STACK_ADDR_FOR_PTHREAD, pcb[pcb_index].pgdir, &pcb[pcb_index], TRUE);
    uint64_t roofs = s_vaddress + PAGE_SIZE; 

    ptr_t pt_argvs = add_user_stack(roofs, roof2, &pcb[pcb_index], 1, argv);
    ptr_t pt_argvs_pthread;
    pt_argvs_pthread = (ptr_t)( ( (char **)pt_argvs )[0][0] );

    //Load the task, return the app_address
    uint64_t app_address = func;

    add_pcb_stack(roof1, pt_argvs_pthread, app_address, &pcb[pcb_index], 0, true);

    pcb[pcb_index].pid = valid_pid;
    pcb[pcb_index].cursor_x = 0;
    pcb[pcb_index].cursor_y = 0;
    pcb[pcb_index].status = TASK_READY;
    pcb[pcb_index].wakeup_time = 0;
    Enqueue(&ready_queue, &pcb[pcb_index].list);
    //Print_queue(&ready_queue);
    //while(1);
}

void do_process_show(void)
{
    char pcb_status[5][10] = {"BLOCKED", "RUNNING", "READY", "EXISTED"};
    printk("[Process Table]:\n");
    int i,j;
    for(i=0,j=0; i<NUM_MAX_TASK; i++)
    {
        if(pcb[i].pcb_valid == 1 && pcb[i].status != TASK_EXITED)
        {
            if(j<9)
            {
                printk("[%d]  PID : %d  STATUS : %s  (name: %s) mask: %d  core_id: %d\n", j, pcb[i].pid, pcb_status[pcb[i].status], pcb[i].name, pcb[i].mask, pcb[i].core_id);
            }
            else if(j == 9)
            {
                printk("[%d]  PID : %d STATUS : %s  (name: %s) mask: %d  core_id: %d\n", j, pcb[i].pid, pcb_status[pcb[i].status], pcb[i].name, pcb[i].mask, pcb[i].core_id);
            }
            else
            {
                printk("[%d] PID : %d STATUS : %s  (name: %s) mask: %d  core_id: %d\n", j, pcb[i].pid, pcb_status[pcb[i].status], pcb[i].name, pcb[i].mask, pcb[i].core_id);
            }
            j++;
        }
    }
}

void do_clear(void)
{
    return;
}

pid_t do_exec(char *name, int argc, char *argv[])
{
    //keep same mask between father and sons
    //we set 3 as mask of shell, so if shell call do_exec, mask will be 3
    pid_t p = add_pcb(name, argc, argv, current_running->mask);
    if(p != 0)
        do_scheduler();
    return p;
}

void do_exit(void)
{
    current_running->status = TASK_EXITED;
    //recovery pid
    recover_pid(current_running->pid);
    //recovKernelPage(current_running->numpage_of_kernel, current_running->kernel_page_index);
    //recovUserpage(current_running->numpage_of_user, current_running->user_page_index);
    current_running->pcb_valid = 0;

    list_node_t *node_tmp;

    while((node_tmp = Dequeue(&current_running->wait_list)) != NULL)
    {

        pcb_t *wait_over = (pcb_t *)((char *)(node_tmp) - 0x10);
        //consider the situation that wait_over has beened killed during waiting
        if(wait_over->status == TASK_BLOCKED)
        {
            wait_over->status = TASK_READY;
            Enqueue(&ready_queue, node_tmp);
        }
    }

    //recycle page
    freePage(current_running);

    do_scheduler();
}

int do_kill(pid_t pid)
{
    for(int i=0; i<NUM_MAX_TASK; i++)
    {
        if(pcb[i].pid == pid)
        {
            //printk("want to kill %s\n",pcb[i].name);
            pcb[i].status = TASK_EXITED;

            //recovery pid
            recover_pid(pcb[i].pid);
            //recovKernelPage(pcb[i].numpage_of_kernel ,pcb[i].kernel_page_index);
            //recovUserpage(pcb[i].numpage_of_user, pcb[i].user_page_index);
            pcb[i].pcb_valid = 0;

            list_node_t *node_tmp;

            //release waitpid
            while((node_tmp = Dequeue(&pcb[i].wait_list)) != NULL)
            {
                pcb_t *wait_over = (pcb_t *)((char *)(node_tmp) - 0x10);
                Print_queue(&pcb[i].wait_list);
                wait_over->status = TASK_READY;
                Enqueue(&ready_queue, node_tmp);
            }

            //release clock
            for(int j=0; j<10; j++)
            {
                //printk("pcb[%d].hold_lock_id[%d] is %d\n",i,j,pcb[i].hold_lock_id[j]);
                if(pcb[i].hold_lock_id[j] != 0)
                {
                    //printk("release lock %d\n",pcb[i].hold_lock_id[j]);
                    kill_process_lock_release(pcb[i].hold_lock_id[j]);
                    pcb[i].hold_lock_id[j] = 0;
                }
            }

            //recycle page
            freePage(&pcb[i]);

            return 1;
        }
    }
    return 0;
}

int do_waitpid(pid_t pid)
{
    for(int i=0; i<NUM_MAX_TASK; i++)
    {
        if(pcb[i].pid == pid && pcb[i].status != TASK_EXITED)
        {
            current_running->status = TASK_BLOCKED;
            printl("task %s is waiting!\n",current_running->name);
            Enqueue(&pcb[i].wait_list, &current_running->list);
            /*
            printl("current_running is %s, turn it ro blocked\n",current_running->name);
            printl("waitpid is %d, name is %s\n",pid,pcb[i].name);
            Print_queue(&pcb[i].wait_list);
            */
            do_scheduler();
            return pid;
        }
    }
    return 0;
}

pid_t do_getpid(void)
{
    return current_running->pid;
}

void taskset_exec(int argc, char *argv[])
{
    int if_exec;
    int mask;
    char *name;
    int pid;
    if(argv[1] == NULL)
    {
        printk("wrong command!\n");
        return;
    }
    else
        if_exec = (strcmp("-p", argv[1]) != 0);

    if(if_exec)
    {
        mask = argv[1][2] - '0';
        name = argv[2];
        int r_argc = argc - 2;
        char **r_argv = argv + 2;
        pid_t p = add_pcb(name, r_argc, r_argv, mask);
    }
    else
    {
        mask = argv[2][2] - '0';
        char *str_pid = argv[3];

        int index = 0;
        while(str_pid[index]!='\0')
        {
            pid = (pid * 10) + (str_pid[index] - '0');
            index++;
        }
        taskset(mask, pid);
    }
}

void taskset(int mask, int pid)
{
    for(int i=0; i<NUM_MAX_TASK; i++)
    {
        if(pcb[i].pid == pid)
        {
            pcb[i].mask = mask;
        }
    }
}

void do_pagefault(uint64_t exp_address)
{   
    //首先希望得到exp_address对应哪一页
    uint64_t base_addr = get_base_addr(exp_address);
    printl("page_fault address: %x\n", base_addr);

    for(int i=0; i < MAX_SD_PAGE_NUM; i++)
    {
        if( (sdpages_mm[i].page_valid == USED) &&
            (base_addr == sdpages_mm[i].round_user_addr) &&
            (current_running->pgdir == sdpages_mm[i].pgdir) )
        {
            //printl("\norigin:\n");
            //show_addr(current_running);

            for(int j=0; j < current_running->num_of_page; j++)
            {
                if(current_running->page_addr[j] == sdpages_mm[i].pages_mm_index)
                {
                    current_running->kind[j] = 3;
                    break;
                }
            }

            //printl("\nafter cancel:\n");
            //show_addr(current_running);

            ptr_t new_addr = alloc_page_helper(base_addr, current_running->pgdir,
                                                current_running, FALSE);
            bios_sdread(kva2pa(new_addr), 8, sdpages_mm[i].block_index);

            //printk("swap! user_address is %lx, new address is %x\n",exp_address,new_addr);

            //将该sd位置无效
            sdpages_mm[i].page_valid = FREE;
            sdpages_mm[i].block_index = 0;
            sdpages_mm[i].pgdir = 0;
            sdpages_mm[i].round_user_addr = 0;

            //printl("\nafter realloc:\n");
            //show_addr(current_running);

            return;
        }
    }

    if(check_if_snp(exp_address, current_running->pgdir))
    {
        //printk("\ncan't write!\n");
        ptr_t pgdir = current_running->pgdir;
        uint64_t ppn = get_page_addr(exp_address);
        //printk("ppn is %x\n", ppn);
        uint64_t vpn = pa2kva(ppn);
        //printk("round vpn is %x\n", vpn);
        make_page_invalid(exp_address, pgdir);
        //printk("make it invalid\n");
        uint64_t new_addr = alloc_page_helper(exp_address, pgdir, current_running, FALSE);
        //printk("new address is %x\n",new_addr);
        memcpy((unsigned char *)new_addr, (unsigned char *)vpn, PAGE_SIZE);
        //printk("copy right!\n");
        shpages_mm[5].origin_va = new_addr;
    }
    else
    {
        alloc_page_helper(exp_address, current_running->pgdir, current_running, FALSE);
    }
}

void show_addr(pcb_t *pcb)
{
    printl("%s hold %d pags:\n", current_running->name, current_running->num_of_page);
    for(int i=0; i<pcb->num_of_page; i++)
    {
        if(current_running->kind[i] == 0)
            printl("pgdir %d: %d\n", i+1, pcb->page_addr[i]);
        else if(current_running->kind[i] == 1)
            printl("kernpage %d: %d\n", i+1, pcb->page_addr[i]);
        else if(current_running->kind[i] == 2)
            printl("ppage %d: %d\n", i+1, pcb->page_addr[i]); 
        else
            printl("cancel page %d: %d\n", i+1, pcb->page_addr[i]);
    }
}
