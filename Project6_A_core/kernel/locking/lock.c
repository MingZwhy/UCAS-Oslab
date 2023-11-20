#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARRIER_NUM];
condition_t conditions[CONDITION_NUM];
mailbox_t mailboxs[MBOX_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    while (lock->status == LOCKED);
    return 1;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    lock->status == LOCKED;
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int mlock_idx = key;
    mlocks[mlock_idx].key = key;
    list_node_t *self = &mlocks[mlock_idx].block_queue;
    mlocks[mlock_idx].block_queue.prev = self;
    mlocks[mlock_idx].block_queue.next = self;
    mlocks[mlock_idx].status = UNLOCKED;
    return mlock_idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    while (1)
    {
        mutex_lock_t *mute_lock = &mlocks[mlock_idx];
        if (mute_lock->status == UNLOCKED)
        {
            mute_lock->status = LOCKED;
            for (int i = 0; i < 16; i++)
            {
                if (current_running->lock[i] == 0)
                {
                    current_running->lock[i] = mlock_idx;
                    break;
                }
            }
            break;
        }
        else
        {
            do_block(&(mute_lock->block_queue), &current_running->list);
            // do_mutex_lock_acquire(mlock_idx);
        }
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mutex_lock_t *mute_lock = &mlocks[mlock_idx];
    for (int i = 0; i < 16; i++)
    {
        if (current_running->lock[i] == mlock_idx)
        {
            current_running->lock[i] = 0;
            break;
        }
    }
    if (isempyt_queue(&(mute_lock->block_queue)))
    {
        mute_lock->status = UNLOCKED;
    }
    else
    {
        mute_lock->status = UNLOCKED;
        do_unblock(&(mute_lock->block_queue));
    }
}

void kill_lock_release(int mlock_idx)
{
    mutex_lock_t *mute_lock = &mlocks[mlock_idx];
    if (isempyt_queue(&(mute_lock->block_queue)))
    {
        mute_lock->status = UNLOCKED;
    }
    else
    {
        mute_lock->status = UNLOCKED;
        do_unblock(&(mute_lock->block_queue));
    }
}

int do_barrier_init(int key, int goal)
{
    int bar_idx = key % BARRIER_NUM;
    barriers[bar_idx].goal = goal;
    barriers[bar_idx].done = 0;
    init_head(&barriers[bar_idx].br_block_queue);
    return bar_idx;
}

void do_barrier_wait(int bar_idx)
{
    barriers[bar_idx].done += 1;
    if (barriers[bar_idx].goal > barriers[bar_idx].done)
    {
        do_block(&barriers[bar_idx].br_block_queue, &current_running->list);
    }
    else
    {
        do_all_unblock(&barriers[bar_idx].br_block_queue);
        barriers[bar_idx].done = 0;
    }
}

void do_barrier_destroy(int bar_idx)
{
    barriers[bar_idx].goal = 0;
    barriers[bar_idx].done = 0;
    init_head(&barriers[bar_idx].br_block_queue);
}

int do_condition_init(int key)
{
    int cond_idx = key % CONDITION_NUM;
    init_head(&conditions[cond_idx].cd_block_queue);
    return cond_idx;
}

void do_condition_wait(int cond_idx, int mutex_idx)
{
    //放锁
    do_mutex_lock_release(mutex_idx);
    //阻塞
    do_block(&(conditions[cond_idx].cd_block_queue), &current_running->list);
    //重新获得锁
    do_mutex_lock_acquire(mutex_idx);

    return;
}

void do_condition_signal(int cond_idx)
{
    do_unblock(&(conditions[cond_idx].cd_block_queue));
    return;
}

void do_condition_broadcast(int cond_idx)
{
    do_all_unblock(&(conditions[cond_idx].cd_block_queue));
    return;
}

void do_condition_destroy(int cond_idx)
{
    init_head(&(conditions[cond_idx].cd_block_queue));
    return;
}

void init_mbox(char *name, int mbox_idx)
{
    memset(mailboxs[mbox_idx].name, 0, MAX_NAME_LEN);
    memset(mailboxs[mbox_idx].buff, 0, MAX_MBOX_LENGTH);
    memcpy(mailboxs[mbox_idx].name, name, MAX_NAME_LEN);
    mailboxs[mbox_idx].tail = 0;
    mailboxs[mbox_idx].user_cnt = 0;
    mailboxs[mbox_idx].used = 1;
    mailboxs[mbox_idx].lock_idx = do_mutex_lock_init(mbox_idx);
    mailboxs[mbox_idx].cond_dec_idx = do_condition_init(mbox_idx);
    mailboxs[mbox_idx].cond_dec_idx = do_condition_init(mbox_idx + 3);
}

int do_mbox_open(char *name)
{
    for (int i = 0; i < MBOX_NUM; i++)
    {
        if (strcmp(name, mailboxs[i].name) == 0)
        {
            if (mailboxs[i].used == 1)
                return i;
        }
    }
    int mbox_idx;
    for (mbox_idx = 0; mbox_idx < MBOX_NUM; mbox_idx++)
    {
        if (mailboxs[mbox_idx].used == 0)
        {
            break;
        }
    }
    init_mbox(name, mbox_idx);
    return mbox_idx;
}

void do_mbox_close(int mbox_idx)
{
    if (mailboxs[mbox_idx].user_cnt > 0)
    {
        mailboxs[mbox_idx].user_cnt--;
        if (mailboxs[mbox_idx].user_cnt == 0)
        {
            mailboxs[mbox_idx].used = 0;
        }
    }
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    do_mutex_lock_acquire(mailboxs[mbox_idx].lock_idx);
    while (mailboxs[mbox_idx].tail+msg_length>MAX_MBOX_LENGTH)
    {
        //再加就满了(ಥ﹏ಥ)
        do_condition_wait(mailboxs[mbox_idx].cond_dec_idx,mailboxs[mbox_idx].lock_idx);
    }

    memcpy((char *)(mailboxs[mbox_idx].buff + mailboxs[mbox_idx].tail),msg,msg_length);
    mailboxs[mbox_idx].tail+=msg_length;

    do_condition_broadcast(mailboxs[mbox_idx].cond_inc_idx);

    do_mutex_lock_release(mailboxs[mbox_idx].lock_idx);

    return 0;
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    do_mutex_lock_acquire(mailboxs[mbox_idx].lock_idx);
    while (mailboxs[mbox_idx].tail - msg_length < 0)
    {
        //真的没有了 (っ˘ڡ˘ς)
        do_condition_wait(mailboxs[mbox_idx].cond_inc_idx,mailboxs[mbox_idx].lock_idx);
    }
    memcpy(msg,mailboxs[mbox_idx].buff, msg_length);
    memcpy(mailboxs[mbox_idx].buff, (char *)(mailboxs[mbox_idx].buff + msg_length),mailboxs[mbox_idx].tail-msg_length);
    mailboxs[mbox_idx].tail-=msg_length;

    do_condition_broadcast(mailboxs[mbox_idx].cond_dec_idx);

    do_mutex_lock_release(mailboxs[mbox_idx].lock_idx);

    return 0;
}
