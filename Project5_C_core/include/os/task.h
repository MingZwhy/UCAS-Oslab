#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    /*
    char file_name[20];
    int offset;
    int file_size;
    */

    //in prj4, we need more info of task
    int offset;
    int file_size;   //在ELF中的大小
    int mem_size;    //在虚拟卡空间中的大小(>=filesz)
    int vaddr;       //第一个字节在进程虚拟空间的起始地址
    char file_name[20];
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif
