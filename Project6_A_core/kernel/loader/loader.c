#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <os/sched.h>

#define TASKNUM_LOC 0x502001fa
#define BLOCK_NUM_OF_PAGE 8
#define PAGE_SIZE 4096

uint64_t load_task_img(
    int taskid,  int pcb_id
)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    
    char *tasknum = TASKNUM_LOC;
    unsigned blockid = tasks[taskid].start_block;
    unsigned blocknm = tasks[taskid].num_block;
    int offset       = tasks[taskid].offset;
    int mem_block    = tasks[taskid].mem_size / PAGE_SIZE + ((tasks[taskid].mem_size % PAGE_SIZE) != 0);
    // unsigned mem_address = TASK_MEM_BASE + (taskid - 1) * TASK_SIZE;
    // unsigned ret_address = mem_address + tasks[taskid].offset;
    uintptr_t v_entrypoint = tasks[taskid].vaddr;
    unsigned  v_blockid    = blockid;
    uint64_t ret_address;
    for(int i=0;i<blocknm;i += BLOCK_NUM_OF_PAGE)
    {
        uintptr_t v_kernel_address = alloc_page_helper(v_entrypoint,pcb[pcb_id].pgdir,0,0);
        if(i == 0)
            ret_address = v_kernel_address;
        bios_sdread(kva2pa(v_kernel_address),BLOCK_NUM_OF_PAGE,v_blockid);
        v_entrypoint += PAGE_SIZE;
        v_blockid += BLOCK_NUM_OF_PAGE;
    }
    for(int i=0;i<blocknm;i+= BLOCK_NUM_OF_PAGE)
    {
        memcpy(ret_address + i *PAGE_SIZE,ret_address+ i *PAGE_SIZE,4096);
    }
    return tasks[taskid].vaddr;
}

uint64_t load_address(int taskid)
{
    return tasks[taskid].vaddr;
}