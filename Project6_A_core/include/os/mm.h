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

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_USER_STACK 0x50500000
#define INIT_KERNEL_STACK_1 0xffffffc053600000
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+PAGE_SIZE)
#define FREEMEM_USER INIT_USER_STACK


#define USER_SP   0xf00010000
#define USER_SP2  0xf00020000
#define MAIN_DIR 0xffffffc051000000
#define USER_FREEMEM_KERNEL 0xffffffc052001000

#define PID0_SP 0xffffffc051001000
#define PID1_SP 0xffffffc051002000

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

//extern ptr_t allocPageDir(int numPage);
//extern ptr_t allocKernelPage(int numPage);
//extern ptr_t allocUserPage(int numPage);
extern ptr_t allocPage(int numPage, ptr_t user_address, ptr_t pgdir);
extern ptr_t allocFixedPage(int numPage, ptr_t pgdir);
extern void  freePage(ptr_t pgdir);

extern void *kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
//type = 1时，是可以swap的内存，type = 2时，利用原来传递的kva构建映射，其余均为不可swap的内存
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, uintptr_t kva, int type);
extern ptr_t get_page_addr(ptr_t va);
void clear_pagevalid(uint64_t va, ptr_t pgdir);
extern ptr_t do_snapget(int key);
int srch_mapping(uint64_t va, ptr_t pgdir);
#define PAGE_NUM 64
#define PAGEDIR_NUM 128
#define PAGEUSER_NUM 512
#define PAGEKERNEL_NUM 128
#define SDPAGE_NUM 128
#define PPAGE_NUM 8
#define FPAGE_NUM 1024

//下面定义物理页的结构体
typedef struct sdpage
{
    /* data */
    ptr_t user_address;
    int   block_id;
    int   valid;
    ptr_t pgdir;
}sdpage;
typedef struct ppage
{
    /* data */
    ptr_t user_address;
    ptr_t kernel_address; 
    int   valid;
    int   fifo;
    ptr_t pgdir;
}ppage;

typedef struct fpage
{
    /* data */
    int   valid;
    ptr_t pgdir;
}fpage;
extern sdpage   sdpage_array[SDPAGE_NUM];
extern ppage    ppage_array [PPAGE_NUM ];
extern fpage    fpage_arrary[FPAGE_NUM];
extern int used_ppage;
extern int used_sdpage;
extern int used_fpage;

//P4-task5
#define SHMPAGE_NUM 64
typedef struct shmpage
{
    int key;
    int valid;
    ptr_t kernel_address;
    ptr_t user_address [16];
    int pid_num;
}shmpage;
extern shmpage shmpage_array[SHMPAGE_NUM];

extern ptr_t do_shmpageget(int key);
extern void  do_shmpagedt(void *addr);
extern void init_shmpage();


#endif /* MM_H */
