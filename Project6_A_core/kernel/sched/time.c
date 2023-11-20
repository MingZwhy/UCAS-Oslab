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

    while (get_timer() - begin_time < time)
        ;
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_node_t *last = sleep_queue.prev;
    while (!isempyt_queue(&sleep_queue))
    {
        list_node_t *node = de_queue(&sleep_queue);
        pcb_t *testing = (pcb_t *)((ptr_t)node - 0x10);
        if (get_timer() >= testing->wakeup_time)
        {
            if (testing->status == TASK_BLOCKED)
            {
                testing->status = TASK_READY;
                testing->wakeup_time = 0;
                en_queue(&ready_queue, node);
            }
        }
        else
        {
            en_queue(&sleep_queue, node);
        }
        if (node == last)
            break;
    }
}