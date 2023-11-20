#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

//uint64_t load_task_img(int taskid)
uint64_t load_task_img(int taskid)
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
    //bios_putstr(tasks[i].file_name);
    mem_address = TASK_MEM_BASE+i*TASK_SIZE;
    block_id = tasks[i].offset / 512;
    //bios_putchar('\n');
    app_address = mem_address + (tasks[i].offset - block_id * 512);
    //num_of_blocks = tasks[i].file_size / 512 + ((tasks[i].file_size % 512) != 0);
    ////num_of_blocks += 1;
    int end_address = tasks[i].file_size + tasks[i].offset;
    int end_block = end_address / 512 + ( (end_address % 512) != 0 );
    num_of_blocks = end_block - block_id;

    //bios_putstr(num_of_blocks + '0');

    //bios_putstr("num_of_blocks are ");
    //bios_putchar(num_of_blocks + '0');
    //bios_putchar('\n');


    bios_sdread(mem_address,num_of_blocks,block_id);
    return app_address;
}
