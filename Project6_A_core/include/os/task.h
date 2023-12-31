#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE 0x52000000
#define TASK_MAXNUM 16

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct
{
    char name[16];
    short start_block;
    short num_block;
    int offset;
    int file_size;
    int mem_size;
    int vaddr;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif
