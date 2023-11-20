#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <os/mm.h>
#include <os/sched.h>
#include <pgtable.h>

//uint64_t load_task_img(int taskid)
uint64_t 
load_task_img(int taskid, ptr_t pgdir, pcb_t *ptr_to_pcb)
{
    /*
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
    */
    int memsz_need_pg_num;
    memsz_need_pg_num = tasks[taskid].mem_size / PAGE_SIZE + ((tasks[taskid].mem_size % PAGE_SIZE) != 0);

    int num_of_blocks, start_block_id, end_block_id;
    int end_address = tasks[taskid].file_size + tasks[taskid].offset;

    start_block_id = tasks[taskid].offset / 512;
    end_block_id = end_address / 512;
    num_of_blocks = end_block_id - start_block_id + 1;
    //left为后续需要将页左移对齐的长度
    int left = tasks[taskid].offset - start_block_id * 512;
    //printl("\n\n\noffset is %d, left is %d\n\n\n",tasks[taskid].offset, left);

    /*
    每个扇区512bytes，1页4KB，因此一页能容纳8个扇区
    */
    int num_of_page = (num_of_blocks / 8) + ((num_of_blocks % 8) != 0);

    ptr_t current_va;
    current_va = tasks[taskid].vaddr;
    ptr_t kernel_va[16];
    int kva_index = 0;
    for(int i=0; i < 16; i++)
    {
        kernel_va[i] = 0;
    }

    int temp = num_of_page;
    //printl("temp is %d\n", temp);
    while(temp>1)
    {
        kernel_va[kva_index] = alloc_page_helper(current_va, pgdir, ptr_to_pcb, TRUE);
        //一页能容纳8个扇区，因此一次页只能sdread 8个扇区
        bios_sdread(kva2pa(kernel_va[kva_index]), 8, start_block_id);
        kva_index++;                //更新index
        start_block_id += 8;        //更新开始扇区
        current_va += PAGE_SIZE;    //更新虚地址

        temp--;                     //需要分配的页减一;
    } 

    //printl("kva_index is %d\n",kva_index);

    //对最后一页，很可能不是需要8个扇区
    int rest_blocks = (num_of_blocks % 8);
    kernel_va[kva_index] = alloc_page_helper(current_va, pgdir, ptr_to_pcb, TRUE);
    bios_sdread(kva2pa(kernel_va[kva_index]), rest_blocks, start_block_id);
    kva_index++;
    start_block_id += 8;
    current_va += PAGE_SIZE;

    //现在我们让每一页的内容都对齐
    /*
    方法是每一页都左移left的长度,这样就做到了整体左移left的长度
    */

    /*
      ----------------------------------------------------------------------------
     /  left   /   page0     /       page1           / ...... /   page[index-1] /
    ----------------------------------------------------------------------------
    */

    for(int i=0; i<kva_index-1; i++)
    {
        //i<kva_index-1是因为对最后一页，只需要做当前页左移的操作
        memcpy((unsigned char *)kernel_va[i],
        (unsigned char *)(kernel_va[i] + left), PAGE_SIZE - left);

        memcpy((unsigned char *)(kernel_va[i] + (PAGE_SIZE - left)),
        (unsigned char *)kernel_va[i+1], left);
    }
    //for last page, just left move
    memcpy((unsigned char *)kernel_va[kva_index-1],
    (unsigned char *)(kernel_va[kva_index-1] + left), PAGE_SIZE - left);


    //清零file_size后页的剩余部分
    /*
    need_zero_len可以理解为本身file_size在最后一页的剩余长度 减去 左移长度left
    */
    int need_zero_len = tasks[taskid].file_size - (kva_index-1)*PAGE_SIZE;
    //printl("need_zero_len is %d\n", need_zero_len);
    /*
    由于上面我们已经将内存整体左移，有可能本身最后一页的内容长度是小于left的
    此时need_zero_len<0，此时清零倒数第二页即可.
    */
    
    
    if(need_zero_len >= 0)
    {
        //正常清空最后一页的剩余部分
        //printl("clear left space for %d\n",need_zero_len);
        bzero((unsigned char *)(kernel_va[kva_index-1] + need_zero_len), PAGE_SIZE - need_zero_len);
    }
    else
    {
        //清空倒数第二页的剩余部分
        int pos_len = 0 - need_zero_len;  //取反
        //printl("pos len is %d\n",pos_len);
        bzero((unsigned char *)(kernel_va[kva_index-2] + PAGE_SIZE - pos_len), pos_len);
    }
    

    //有一定可能bss段导致需要多分配一页
    /*
    我们将这一判断放在最后进行，是因为可能在清零file_size后页的剩余部分
    的时候，就已经完成这一工作了。
    */
    
    int addition_page = memsz_need_pg_num - kva_index;
    //printl("additional_page is %d\n", addition_page);
    while(addition_page > 0)
    {
        kernel_va[kva_index] = alloc_page_helper(current_va, pgdir, ptr_to_pcb, TRUE);
        //bss段，全部置0
        bzero((unsigned char *)kernel_va[kva_index], PAGE_SIZE);
        kva_index++;
        addition_page--;
        current_va += PAGE_SIZE;
    }

    return tasks[taskid].vaddr;
}

uint64_t load_task_img_j(int taskid)
{
    return TASK_MEM_BASE+taskid*TASK_SIZE;
}