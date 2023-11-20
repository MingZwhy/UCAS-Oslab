#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_node_t * tail = sleep_queue.prev;

    while(!if_queue_empty(&sleep_queue))
    {
        list_node_t * node = Dequeue(&sleep_queue);
        pcb_t * pcb = (pcb_t *)( (ptr_t)node - 2 * sizeof(reg_t));
        if(get_timer() >= pcb->wakeup_time)
        {
            //consider the situation that process has been killed when sleeping
            if(pcb->status == TASK_BLOCKED)
            {
                //已达到唤醒时间，令其加入ready_queue
                pcb->status = TASK_READY;
                pcb->wakeup_time = 0;
                Enqueue(&ready_queue, node);
            }
        }
        else
        {
            //尚未达到时间，重新加入sleep_queue
            Enqueue(&sleep_queue, node);
        }

        if(node == tail)
        {
            //已完成一个循环
            break;
        }
    }
}