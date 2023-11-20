#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

/*
typedef enum {
    UNLOCKED,
    LOCKED,
} lock_status_t;

typedef struct spin_lock
{
    volatile lock_status_t status;
} spin_lock_t;

typedef struct mutex_lock
{
    spin_lock_t lock;
    list_head block_queue;
    int key;
} mutex_lock_t;
*/

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    //init status --> UNLOCKER
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return(lock->status == UNLOCKED);
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    lock->status = LOCKED;
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int mlock_idx = key % LOCK_NUM;

    mlocks[mlock_idx].key = key;
    //init block_queue for this lock
    list_node_t * self = &mlocks[mlock_idx].block_queue;
    mlocks[mlock_idx].block_queue.prev = self;
    mlocks[mlock_idx].block_queue.next = self;
    spin_lock_init(&mlocks[mlock_idx].lock);
    return mlock_idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    if(spin_lock_try_acquire(&mlocks[mlock_idx].lock))
    {
        spin_lock_acquire(&mlocks[mlock_idx].lock);
    }
    else
    {
        //void do_block(list_node_t *pcb_node, list_head *queue)
        do_block(&current_running->list, &mlocks[mlock_idx].block_queue);
        //now we should switch_to

        list_node_t * next_listnode = Dequeue(&ready_queue);
        pcb_t * next_pcb = (pcb_t *)( (ptr_t)next_listnode - 2 * sizeof(reg_t) );
        pcb_t * prev_pcb = current_running;
        next_pcb->status = TASK_RUNNING;
        current_running = next_pcb;

        switch_to(prev_pcb,next_pcb);
        //this proc acquire lock failed last time and be put in blocked queue
        //when it is released from blocked queue which means this lock is unlocked now
        //it should acquire lock!
        do_mutex_lock_acquire(mlock_idx);
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    spin_lock_release(&mlocks[mlock_idx].lock);
    do_unblock(&mlocks[mlock_idx].block_queue);
}
