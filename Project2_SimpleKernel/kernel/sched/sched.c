#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
pcb_t tcb[NUM_MAX_THREAD];

//using in thread_create
#define SR_SPIE   0x00000020 /* Previous Supervisor IE */
extern void ret_from_exception();

const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    /*------------------------debug----------------------------*/

    printl("begin to do_scheduler:\n\n");
    Print_queue(&ready_queue);
    
    
    /*---------------just for showing some info----------------*/

    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();

    // TODO: [p2-task1] Modify the current_running pointer.


    // TODO: [p2-task1] switch_to current_running

    //将队头元素出队
    list_node_t * next_listnode = Dequeue(&ready_queue);

    //printl("After scheduler dequeue: \n");
    //Print_queue(&ready_queue);

    pcb_t * next_pcb = (pcb_t *)( (ptr_t)next_listnode - 2 * sizeof(reg_t) );
    //printl("next_pcb is %x\n",next_pcb);
    pcb_t * prev_pcb = current_running;
    //printl("prev_pcb is %x\n",prev_pcb);
    
    //调整状态
    next_pcb->status = TASK_RUNNING;
    if(prev_pcb->status == TASK_RUNNING)
    {    
        //consider the sleep
        //if prev_pcb->statue == TASK_BLOCKED, We can't set it ready
        prev_pcb->status = TASK_READY;
    }
    //将刚退下的任务入队尾
    //但要排除pid0
    if( (prev_pcb != &pid0_pcb) && (prev_pcb->status == TASK_READY) )
    {
        Enqueue(&ready_queue, &prev_pcb->list);
        //printl("After scheduler dequeue: \n");
        //Print_queue(&ready_queue);
    }
    //将current_running调整为刚出队的任务
    current_running = next_pcb;

    /*------------------------debug----------------------------*/
    /*
    printl("prev_pcb kernel_sp: %x\n", prev_pcb->kernel_sp);
    switchto_context_t *temp =
        (switchto_context_t *)((ptr_t)prev_pcb->kernel_sp);
    printl("temp is %x\n",temp);
    printl("prev_pcb RA: %x\n", temp->regs[0]);
    printl("prev_pcb sp: %x\n", temp->regs[1]);

    printl("next_pcb kernel_sp: %x\n", next_pcb->kernel_sp);
    temp = (switchto_context_t *)((ptr_t)next_pcb->kernel_sp);
    printl("temp is %x\n",temp);
    printl("next_pcb RA: %x\n", temp->regs[0]);
    printl("next_pcb sp: %x\n", temp->regs[1]);
    */
    /*---------------just for showing some info----------------*/

    printl("into switch_to\n");
    printl("next running task is %s\n",current_running->name);
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
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    //do_unblock(&mlocks[mlock_idx].block_queue);
    list_node_t * node = Dequeue(pcb_node);
    while(node != NULL)
    {
        pcb_t * cur_pcb = (pcb_t *)( (ptr_t)node - 2 * sizeof(reg_t) );
        cur_pcb->status = TASK_READY;
        Enqueue(&ready_queue,node);
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
    bottom1 = allocKernelPage(1);
    roof1 = bottom1 + pcb[pid-1].numpage_of_kernel * PAGE_SIZE;
    bottom2 = allocUserPage(1);
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
