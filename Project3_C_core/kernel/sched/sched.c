#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#include <os/task.h>    //for task_intfo_t tasks

int if_first_do_sche = 1;

pcb_t pcb[NUM_MAX_TASK];
pcb_t tcb[NUM_MAX_THREAD];

//using in thread_create
#define SR_SPIE   0x00000020 /* Previous Supervisor IE */
extern void ret_from_exception();

const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .name = "empty_m"
};

const ptr_t pidn_stack = NEXT_KERNEL_STACK + PAGE_SIZE;
pcb_t pidn_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pidn_stack,
    .user_sp = (ptr_t)pidn_stack,
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

void add_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, int argc, char *argv[])
{
    pcb->user_sp = user_stack;
    printl("original address of user_stack is %x\n",pcb->user_sp);
    char  **pt_argvs = (char **)(user_stack - sizeof(uint64_t) * argc);
    pcb->user_sp -= (sizeof(uint64_t) * argc);
    printl("sizeof argvs is %d\n",(sizeof(uint64_t) * argc));
    printl("address of pt_argvs is %x\n",pt_argvs);
    printl("argc is %d\n",argc);
    for(int i=0; i<argc; i++)
    {
        printl("address of argv[%d] is %x\n",i,&pt_argvs[i]);

        int len = strlen(argv[i]);
        printl("len is %d\n",len);
        char * s = (char *)(pcb->user_sp - (len+1));

        pcb->user_sp -= (sizeof(char) * (len+1));
        strcpy(s,argv[i]);

        pt_argvs[i] = s;
        printl("s is %s\n",s);
        printl("content of argv[%d] is %s\n\n",i,pt_argvs[i]);
    }

    //alignment
    int align_num = (user_stack - pcb->user_sp) / 128;
    pcb->user_sp = user_stack - (align_num+1)*128;
    printl("new address of user_stack is %x\n",pcb->user_sp);

    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    pt_regs->regs[1] = &ret_from_exception; 
    pt_regs->regs[2] = pcb->user_sp;

    pt_regs->regs[10] = (uint64_t)argc;           //argc --> a0
    //printk("reg[10] is %d\n",pt_regs->regs[10]);
    pt_regs->regs[11] = (uint64_t)pt_argvs;                 //argv_base --> a1

    /*
    for(int i=0;i<argc;i++)
    {
        printk("%s, %x, %x\n",pt_argvs[i], pt_argvs[i], &pt_argvs[i]);
    }
    */

    pt_regs->sepc = entry_point;
    pt_regs->sstatus = SR_SPIE;

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

            //must init wait_queue first
            init_queue(&pcb[pcb_index].wait_list);

            //init hold_lock
            for(int j=0; j<10; j++)
            {
                pcb[pcb_index].hold_lock_id[j] = 0;
            }

            pcb[pcb_index].mask = mask;
            pcb[pcb_index].core_id = get_current_cpu_id();

            uint64_t app_address = load_task_img_j(i);
            strcpy(pcb[pcb_index].name, tasks[i].file_name);
            printl("i: %d task_name: %s\n",i,pcb[pcb_index].name);

            bottom1 = allocKernelPage(1, &pcb[pcb_index].kernel_page_index);
            pcb[pcb_index].numpage_of_kernel = 1;
            roof1 = bottom1 + pcb[pcb_index].numpage_of_kernel * PAGE_SIZE;

            bottom2 = allocUserPage(1, &pcb[pcb_index].user_page_index);
            pcb[pcb_index].numpage_of_user = 1;
            roof2 = bottom2 + pcb[pcb_index].numpage_of_user * PAGE_SIZE;

            add_pcb_stack(roof1, roof2, app_address, &pcb[pcb_index], argc, argv);

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
    uint64_t ipi = get_current_cpu_id();
    if (ipi == 0)
    {
        current_running = current_running_m;
    }
    else
    {
        current_running = current_running_n;
    }

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

    switch_to(prev_running, next_running);
}

void do_scheduler_m(void)
{
    check_sleeping();

    pcb_t * next_pcb;
    pcb_t * prev_pcb;

    list_node_t * next_listnode = Dequeue(&ready_queue);
    if(next_listnode == NULL)
    {   
        next_pcb = &pid0_pcb;
    }
    else
    {
        next_pcb = (pcb_t *)( (ptr_t)next_listnode - 2 * sizeof(reg_t) );
        if(current_running_m == &pid0_pcb)
        {
            set_timer(get_ticks() + TIMER_INTERVAL);
        }
    }
    
    if(next_pcb->status == TASK_READY)
    {
        next_pcb->status = TASK_RUNNING;
    }

    prev_pcb = current_running_m;
    
    if(prev_pcb->status == TASK_RUNNING)
    {    
        prev_pcb->status = TASK_READY;
    }

    if( (prev_pcb != &pid0_pcb) && (prev_pcb->status == TASK_READY) )
    {
        Enqueue(&ready_queue, &prev_pcb->list);
    }

    current_running_m = next_pcb;
    //printl("m: next running is %s\n",next_pcb->name);

    switch_to(prev_pcb,next_pcb);

}

void do_scheduler_n(void)
{
    check_sleeping();
    
    pcb_t * next_pcb;
    pcb_t * prev_pcb;

    list_node_t * next_listnode = Dequeue(&ready_queue);
    if(next_listnode == NULL)
    {
        next_pcb = &pidn_pcb;
    }
    else
    {
        next_pcb = (pcb_t *)( (ptr_t)next_listnode - 2 * sizeof(reg_t) );
        if(current_running_n == &pidn_pcb)
        {
            set_timer(get_ticks() + TIMER_INTERVAL);
        }
    }
        
    if(next_pcb->status == TASK_READY)
    {
        next_pcb->status = TASK_RUNNING;
    }

    prev_pcb = current_running_n;
    
    if(prev_pcb->status == TASK_RUNNING)
    {    
        prev_pcb->status = TASK_READY;
    }

    if( (prev_pcb != &pidn_pcb) && (prev_pcb->status == TASK_READY) )
    {
        Enqueue(&ready_queue, &prev_pcb->list);
    }

    current_running_n = next_pcb;

    //printl("n: next running is %s\n",next_pcb->name);
    switch_to(prev_pcb,next_pcb);
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

//task5 --> thread_create
void thread_create(ptr_t function, int pid)
{
    /*param
    ptr_t function: 新创建线程要执行的函数
    pid：新创建线程的线程号
    */

    //step1: 为新进程创建其对应的pcb
    
    //step1-1: 分配内核栈和用户栈
    ptr_t bottom1, bottom2, roof1, roof2;
    bottom1 = allocKernelPage(1, &tcb[pid-1].kernel_page_index);
    roof1 = bottom1 + pcb[pid-1].numpage_of_kernel * PAGE_SIZE;
    bottom2 = allocUserPage(1, &tcb[pid-1].numpage_of_user);
    roof2 = bottom2 + pcb[pid-1].numpage_of_user * PAGE_SIZE;

    tcb[pid-1].numpage_of_kernel = 1;
    tcb[pid-1].numpage_of_user = 1;

    regs_context_t *pt_regs = 
        (regs_context_t *)(roof1 - sizeof(regs_context_t));

    pt_regs->regs[1] = (reg_t)function;   //ra
    pt_regs->regs[2] = roof2;             //sp
    pt_regs->sstatus = SR_SPIE;           //status
    pt_regs->sepc    = (reg_t)function;   //sepc

    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    tcb[pid-1].kernel_sp = pt_switchto;
    tcb[pid-1].user_sp = roof2;

    pt_switchto->regs[0] = (reg_t)ret_from_exception;   //ra
    pt_switchto->regs[1] = pt_switchto;                 //sp

    tcb[pid-1].pid = pid;
    tcb[pid-1].status = TASK_READY;
    tcb[pid-1].cursor_x = 0;
    tcb[pid-1].cursor_y = 0;
    //tcb[pid-1].name
    tcb[pid-1].wakeup_time = 0;

    Enqueue(&ready_queue, &tcb[pid-1].list);
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
    recovKernelPage(current_running->numpage_of_kernel, current_running->kernel_page_index);
    recovUserpage(current_running->numpage_of_user, current_running->user_page_index);
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
            recovKernelPage(pcb[i].numpage_of_kernel ,pcb[i].kernel_page_index);
            recovUserpage(pcb[i].numpage_of_user, pcb[i].user_page_index);
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
