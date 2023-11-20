#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

uint64_t load_task_img(/*char *taskname*/int taskid, int pcb_id);
uint64_t load_address(int taskid);
#endif