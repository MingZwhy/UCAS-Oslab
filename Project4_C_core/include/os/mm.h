/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <os/sched.h>

#define FREE 0
#define USED 1

#define bool int
#define TRUE 1
#define FALSE 0

#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define NEXT_KERNEL_STACK (INIT_KERNEL_STACK + PAGE_SIZE)
#define FREEMEM_KERNEL 0xffffffc052030000
#define STUMEM_KERNEL 0xffffffc054000000
#define USERMEM_BASE 0xffffffc055000000
#define KERNMEM_BASE 0xffffffc056000000
#define SHAREMEM_BASE 0xffffffc057000000

#define SHAREMEM_USER_BASE 0x30800000

#define MAX_PAGE_NUM 128
#define MAX_KPAGE_NUM 128
#define MAX_UPAGE_NUM 128
#define MAX_STU_PAGE_NUM 64
#define MAX_SD_PAGE_NUM 128
#define MAX_SH_PAGE_NUM 128
#define EXCHANGE_BLOCK_INDEX 200

/*
in createimage, we use block to about 138
so we set EXCHANGE_BLOCK_INDEX as 200 > 138
*/

#define EXCHANGE_BLOCK_INDEX 200

//for task3
typedef struct pgdir
{
    ptr_t page_addr;
    bool page_valid;
}pgdir, *ptr_to_pgdir;

typedef struct page
{
    ptr_t page_addr;        //页地址
    ptr_t pgdir;
    bool page_valid;        //是否占用
    ptr_t user_addr;        //用于定位0级页表，在换页时取消有效位
    int used_time;          //用于记录该页面被使用的时间
}page, *ptr_to_page;

typedef struct sdpage
{
    ptr_t round_user_addr;  //注意这里是一个取整的地址，这是为了在page_fault中能对应起来 
    ptr_t pgdir;
    bool page_valid;
    int block_index;        //用于其保存在sd卡的扇区
    int pages_mm_index;     //在pages_mm中的index，方便回收
}sdpage, *ptr_to_sdpage;

typedef struct shpage
{
    ptr_t page_addr;
    int share_num;
    ptr_t origin_va;
}shpage, *ptr_to_shpage;

extern pgdir pgdir_mm[MAX_PAGE_NUM];
extern pgdir kpages_mm[MAX_KPAGE_NUM];
extern pgdir upages_mm[MAX_UPAGE_NUM];
extern page pages_mm[MAX_STU_PAGE_NUM];
extern sdpage sdpages_mm[MAX_SD_PAGE_NUM];
extern shpage shpages_mm[MAX_SH_PAGE_NUM];

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

/*
these page alloc are used in prj3, now we use allocPage
extern void init_page_valid(void);
extern ptr_t allocKernelPage(int numPage, int *index);
extern ptr_t allocUserPage(int numPage, int *index);
extern void recovKernelPage(int numPage, int index);
extern void recovUserpage(int numPage, int index);
*/

extern void init_mm(void);
extern ptr_t allocPgdir(int numPage, pcb_t *ptr_to_pcb);
extern ptr_t allocKernPage(int numPage, pcb_t *ptr_to_pcb);
extern ptr_t allocPage(int numPage, pcb_t *ptr_to_pcb, ptr_t vaddr);
extern ptr_t allocShPage(int numPage, pcb_t *ptr_to_pcb);
extern void freePage(pcb_t *pcb);

//#define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core

#define USER_STACK_ADDR 0xf00010000
#define USER_STACK_ADDR_FOR_PTHREAD 0xf00050000
#endif

extern uintptr_t shm_page_get(int key);
extern void shm_page_dt(uintptr_t addr);
extern uintptr_t snp_page_get(int key);

// TODO [P4-task1] */
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb, bool still);
uintptr_t alloc_snappage_helper(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb);
extern void alloc_share_page(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb, ptr_t page_addr);
extern void alloc_snp_page(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb, ptr_t page_addr);
extern ptr_t make_page_invalid(uintptr_t va, uintptr_t pgdir);
extern void make_page_cantwrite(uintptr_t va, uintptr_t pgdir);

extern ptr_t find_valid_addr(uintptr_t pgdir);
extern void make_page_valid(uintptr_t va, uintptr_t pgdir);         //not used 
extern bool check_if_exchanged(uintptr_t va, uintptr_t pgdir, int *block_index);        //not used
extern bool check_if_snp(uintptr_t va, uintptr_t pgdir); 
extern void make_page_cantwrite(uintptr_t va, uintptr_t pgdir);

extern void time_update(void);
extern int choose_ex_page(void);
extern void write_sd(int page_index, ptr_t pgdir);
extern uint64_t get_base_addr(uint64_t addr);
extern ptr_t get_page_addr(ptr_t va);

#endif /* MM_H */
