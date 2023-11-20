#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARR_NUM];
condition_t conditions[COND_NUM];
mailbox_t mailboxs[MAILBOX_NUM];

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
        for(int i=0; i<10 ;i++)
        {
            if(current_running->hold_lock_id[i] == 0)
            {
                current_running->hold_lock_id[i] = mlock_idx;
                break;
            }
        }
    }
    else
    {
        //void do_block(list_node_t *pcb_node, list_head *queue)
        do_block(&current_running->list, &mlocks[mlock_idx].block_queue);
        
        //this proc acquire lock failed last time and be put in blocked queue
        //when it is released from blocked queue which means this lock is unlocked now
        //it should acquire lock!
        do_mutex_lock_acquire(mlock_idx);
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    for(int i=0; i<10 ;i++)
    {
        if(current_running->hold_lock_id[i] == mlock_idx)
        {
            current_running->hold_lock_id[i] = 0;
            break;
        }
    }

    spin_lock_release(&mlocks[mlock_idx].lock);
    do_unblock(&mlocks[mlock_idx].block_queue);
}

//project3
void kill_process_lock_release(int mlock_idx)
{
    spin_lock_release(&mlocks[mlock_idx].lock);
    do_unblock(&mlocks[mlock_idx].block_queue);
}

//barrier
int do_barrier_init(int key, int goal)
{
    int bar_idx = key % BARR_NUM;
    barriers[bar_idx].share_num = goal;
    barriers[bar_idx].reach_num = 0;
    init_queue(&barriers[bar_idx].bs_wait_queue);
    return bar_idx;
}

void do_barrier_wait(int bar_idx)
{
    barriers[bar_idx].reach_num++;
    //printk("now barriers wait num is %d\n",barriers[bar_idx].reach_num);
    if(barriers[bar_idx].reach_num == barriers[bar_idx].share_num)
    {
        //step1: dequeue all wait pid caused by barrier
        list_node_t *node_tmp;
        while((node_tmp = Dequeue(&barriers[bar_idx].bs_wait_queue)) != NULL)
        {
            pcb_t *wait_over = (pcb_t *)((char *)(node_tmp) - 0x10);
            //consider the situation that wait_over has beened killed during waiting
            if(wait_over->status == TASK_BLOCKED)
            {
                wait_over->status = TASK_READY;
                Enqueue(&ready_queue, node_tmp);
            }
        }
        //step2: update the barrier
        barriers[bar_idx].reach_num = 0;
    }
    else
    {
        current_running->status = TASK_BLOCKED;
        Enqueue(&barriers[bar_idx].bs_wait_queue, &current_running->list);
        do_scheduler();
    }
}

void do_barrier_destroy(int bar_idx)
{
    barriers[bar_idx].share_num = 0;
    barriers[bar_idx].reach_num = 0;
    init_queue(&barriers[bar_idx].bs_wait_queue);
}

//condition
int do_condition_init(int key)
{
    int cond_idx = key % BARR_NUM;
    init_queue(&conditions[cond_idx].cd_wait_queue);
    return cond_idx;
}

void do_condition_wait(int cond_idx, int mutex_idx)
{
    //step1 release the lock
    do_mutex_lock_release(mutex_idx);

    //step2 enqueue
    current_running->status = TASK_BLOCKED;
    Enqueue(&conditions[cond_idx].cd_wait_queue, &current_running->list);

    //step3 do_scheduler
    do_scheduler();
}

void do_condition_signal(int cond_idx)
{
    list_node_t *node_tmp;
    node_tmp = Dequeue(&conditions[cond_idx].cd_wait_queue);
    if(node_tmp != NULL)
    {
        pcb_t *wait_over = (pcb_t *)((char *)(node_tmp) - 0x10);
        //consider the situation that it has been killed
        if(wait_over->status == TASK_BLOCKED)
        {
            wait_over->status = TASK_READY;
            Enqueue(&ready_queue, node_tmp);
        }
    }
    //do_scheduler();
}

void do_condition_broadcast(int cond_idx)
{
    list_node_t *node_tmp;
    while((node_tmp = Dequeue(&conditions[cond_idx].cd_wait_queue)) != NULL)
    {
        pcb_t *wait_over = (pcb_t *)((char *)(node_tmp) - 0x10);
        //consider the situation that wait_over has beened killed during waiting
        if(wait_over->status == TASK_BLOCKED)
        {
            wait_over->status = TASK_READY;
            Enqueue(&ready_queue, node_tmp);
        }
    }
    //do_scheduler();
}

int Hash(char *name, int MAX_NUM)
{
    int hash = 0;
    int len = strlen(name);
    for(int i=0; i<len; i++)
    {
        hash += name[i];
    }
    hash = hash % MAX_NUM;
    int origin_hash = hash;
    while(1)
    {
        if(mailboxs[hash].valid == 1)
        {
            if(hash < (MAILBOX_NUM-1))
            {
                hash++;
                if(hash == origin_hash)
                {
                    printk("there is no valid mailbox!\n");
                    return -1;
                }
            }
            else 
                hash = 0;
        }
        else
            break;
    }
    return hash;
}

void do_condition_destroy(int cond_idx)
{
    init_queue(&conditions[cond_idx].cd_wait_queue);
}

void init_mbox(char *name, int mbox_idx)
{
    mailboxs[mbox_idx].valid = 1;
    strcpy(mailboxs[mbox_idx].name, name);

    mailboxs[mbox_idx].user_cnt = 0;
    mailboxs[mbox_idx].buff_used = 0;
    mailboxs[mbox_idx].start = 0;
    mailboxs[mbox_idx].end = 0;

    mailboxs[mbox_idx].lock_idx = do_mutex_lock_init(mbox_idx);
    mailboxs[mbox_idx].cond_not_full_idx = do_condition_init(mbox_idx);
    mailboxs[mbox_idx].cond_not_empty_idx = do_condition_init(mbox_idx+1);
}

//mailbox
int do_mbox_open(char *name)
{
    for(int i=0; i<MAILBOX_NUM; i++)
    {
        if(strcmp(mailboxs[i].name, name) == 0)
        {
            //find this mail_box
            return i;
        }
    }
    int hash = Hash(name, MAILBOX_NUM);

    init_mbox(name, hash);
    return hash;
}

void do_mbox_close(int mbox_idx)
{
    if(mailboxs[mbox_idx].user_cnt > 0)
    {
        mailboxs[mbox_idx].user_cnt--;
        if(mailboxs[mbox_idx].user_cnt == 0)
        {
            mailboxs[mbox_idx].valid = 0;
        }
    }
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length)
{
    do_mutex_lock_acquire(mailboxs[mbox_idx].lock_idx);

    while(mailboxs[mbox_idx].buff_used + msg_length > MAX_MBOX_LENGTH)
    {
        //full! need to wait
        do_condition_wait(mailboxs[mbox_idx].cond_not_full_idx, mailboxs[mbox_idx].lock_idx);
    }

    mailboxs[mbox_idx].buff_used += msg_length;

    uint8_t * from = msg;
    uint8_t * dest = mailboxs[mbox_idx].buff;

    for(int i=0; i<msg_length; i++)
    {
        dest[(mailboxs[mbox_idx].end + i) % MAX_MBOX_LENGTH] = from[i];
    }
    mailboxs[mbox_idx].end = (mailboxs[mbox_idx].end + msg_length) % MAX_MBOX_LENGTH;

    do_condition_broadcast(mailboxs[mbox_idx].cond_not_empty_idx);
    
    do_mutex_lock_release(mailboxs[mbox_idx].lock_idx);
    return 0;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length)
{
    do_mutex_lock_acquire(mailboxs[mbox_idx].lock_idx);

    while(mailboxs[mbox_idx].buff_used - msg_length < 0)
    {
        //not enough! need to wait
        do_condition_wait(mailboxs[mbox_idx].cond_not_empty_idx, mailboxs[mbox_idx].lock_idx);
    }

    mailboxs[mbox_idx].buff_used -= msg_length;

    uint8_t * from = mailboxs[mbox_idx].buff;
    uint8_t * dest = msg;

    for(int i=0; i<msg_length; i++)
    {
        dest[i] = from[(mailboxs[mbox_idx].start + i) % MAX_MBOX_LENGTH];
    }
    mailboxs[mbox_idx].start = (mailboxs[mbox_idx].start + msg_length) % MAX_MBOX_LENGTH;

    do_condition_broadcast(mailboxs[mbox_idx].cond_not_full_idx);

    do_mutex_lock_release(mailboxs[mbox_idx].lock_idx);
    return 0;
}