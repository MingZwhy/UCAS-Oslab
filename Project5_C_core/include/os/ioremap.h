#include <type.h>
#include <os/sched.h>

#ifndef IOREMAP_H
#define IOREMAP_H

// using this as IO address space (at most using 1 GB, so that it can be store in one pgdir entry)
#define IO_ADDR_START 0xffffffe000000000lu

extern void * ioremap(unsigned long phys_addr, unsigned long size);
extern void iounmap(void *io_addr);
extern void make_map(unsigned long va, unsigned long paddr, 
              pcb_t *ptr_to_pcb , ptr_t pgdir);

#endif // IOREMAP_H