#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

//uint64_t load_task_img(int taskid)
uint64_t 
load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    /*  task3
    unsigned num_of_blocks = 15;
    unsigned block_id = 1 + num_of_blocks + (taskid-1)*num_of_blocks;   //bootblocker,kernel,app1,app2,appi
    unsigned mem_address = TASK_MEM_BASE+(taskid-1)*TASK_SIZE;

    sd_read(mem_address,num_of_blocks,block_id);

    return mem_address;
    */

    //bios_putchar('\n');
    uint64_t mem_address, app_address;
    int i, num_of_blocks, block_id;
    i = taskid;
    block_id = tasks[i].offset / 512;
    app_address = TASK_MEM_BASE+i*TASK_SIZE;
    mem_address = app_address - (tasks[i].offset - block_id * 512);
    int end_address = tasks[i].file_size + tasks[i].offset;
    int end_block = end_address / 512 + ( (end_address % 512) != 0 );
    num_of_blocks = end_block - block_id;

    bios_sdread(mem_address,num_of_blocks,block_id);
    return app_address;
}

uint64_t load_task_img_j(int taskid)
{
    return TASK_MEM_BASE+taskid*TASK_SIZE;
}