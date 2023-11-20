#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <os/sched.h>

uint64_t load_task_img(int taskid, ptr_t pgdir, pcb_t *ptr_to_pcb);
uint64_t load_task_img_j(int taskid);

#endif