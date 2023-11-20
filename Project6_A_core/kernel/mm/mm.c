#include <os/mm.h>
#include <pgtable.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <assert.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER;
static int pageValid[PAGE_NUM] = {0};
static void clear_page(uint64_t address);
// static void clear_pagevalid(uint64_t va, ptr_t pgdir);
static void p2sd(int num, ptr_t pgdir);
static int fifo_seek();
static void fifo_updata();
extern int used_ppage = 0;
extern int used_sdpage = 0;
extern int used_fpage = 0;

//这里令5600 0000后的空间分配相应的页目录和页表，这些是映射关系，较为重要，不考虑将其替换到sd卡内
// static ptr_t pagedirMemBase = 0xffffffc056000000;
// static int pagedir_Valid[PAGEDIR_NUM] = {0};
// ptr_t allocPageDir(int numpage)
// {
//     int num;
//     for (num = 0; num < PAGEDIR_NUM; num++)
//     {
//         if (pagedir_Valid[num] == 0)
//         {
//             pagedir_Valid[num] = 1;
//             break;
//         }
//     }
//     ptr_t ret = ROUND(pagedirMemBase + num * PAGE_SIZE, PAGE_SIZE);
//     return ret;
// }

//这里令5680 0000后的地址作为内核栈的分配地址，内核栈相比比较重要，也不应该被移动到SD卡上
// static ptr_t kernelMemBase = 0xffffffc056800000;
static int kernelValid[PAGEKERNEL_NUM] = {0};

// ptr_t allocKernelPage(int numPage)
// {
//     int num;
//     for (num = 0; num < PAGEKERNEL_NUM; num++)
//     {
//         if (kernelValid[num] == 0)
//         {
//             kernelValid[num] = 1;
//             break;
//         }
//     }
//     ptr_t ret = ROUND(kernelMemBase + num * PAGE_SIZE, PAGE_SIZE);
//     return ret;
// }

//这里令5200 0000后的地址作为用户程序和用户栈的分配地址，这些地址可以更改块的数目，进而改变是否需要利用到sd卡
sdpage sdpage_array[SDPAGE_NUM];
ppage ppage_array[PPAGE_NUM];
static ptr_t userMemBase = 0xffffffc052000000;
ptr_t allocPage(int numPage, ptr_t user_address, ptr_t pgdir)
{
    int num;
    for (num = 0; num < PPAGE_NUM; num++)
    {
        if (ppage_array[num].valid == 0)
        {
            ptr_t ret = ROUNDDOWN(userMemBase + num * PAGE_SIZE, PAGE_SIZE);
            clear_page(ret);
            ppage_array[num].valid = 1;
            used_ppage++;
            ppage_array[num].user_address = (user_address >> 12) << 12;
            ppage_array[num].fifo = 1;
            ppage_array[num].pgdir = pgdir;
            fifo_updata();
            ppage_array[num].kernel_address = ret;
            return ret;
        }
    }
    num = fifo_seek();
    p2sd(num, ppage_array[num].pgdir);
    clear_pagevalid(ppage_array[num].user_address, ppage_array[num].pgdir);
    ppage_array[num].valid = 0;
    used_ppage--;
    return allocPage(1, user_address, pgdir);
}
//这里令5600 0000后的地址作为内核栈、用户程序和页表的分配地址，这些页无法被移动到SD卡上
fpage fpage_array[FPAGE_NUM] = {0};
static ptr_t fixedMemBase = 0xffffffc056000000;
ptr_t allocFixedPage(int numPage, ptr_t pgdir)
{
    int num;
    for (num = 0; num < FPAGE_NUM; num++)
    {
        if (0 == fpage_array[num].valid)
        {
            ptr_t ret = ROUNDDOWN(fixedMemBase + num * PAGE_SIZE, PAGE_SIZE);
            clear_page(ret);
            fpage_array[num].valid = 1;
            used_fpage++;
            //????
            if (pgdir == 0)
                fpage_array[num].pgdir = ret;
            else
                fpage_array[num].pgdir = pgdir;
            return ret;
        }
    }
    assert(0);
}
// ptr_t allocUserPage(int numPage)
// {
//     // align PAGE_SIZE
//     int num;
//     for (num = 0; num < 16; num++)
//     {
//         if (userValid[num] == 0)
//         {
//             userValid[num] = 1;
//             break;
//         }
//     }
//     ptr_t ret = ROUND(pageuserMemBase + (num)*PAGE_SIZE, PAGE_SIZE);
//     return ret;
// }
void freePage(ptr_t pgdir)
{
    // for (int i = 0; i < FPAGE_NUM; i++)
    // {
    //     if (fpage_array[i].pgdir == pgdir)
    //     {
    //         fpage_array[i].pgdir = 0;
    //         fpage_array[i].valid = 0;
    //         used_fpage--;
    //     }
    // }
    for (int i = 0; i < PPAGE_NUM; i++)
    {
        if (ppage_array[i].pgdir == pgdir)
        {
            ppage_array[i].user_address = 0;
            ppage_array[i].kernel_address = 0;
            ppage_array[i].pgdir = 0;
            ppage_array[i].fifo = 0;
            ppage_array[i].valid = 0;
            used_ppage--;
        }
    }
    for (int i = 0; i < SDPAGE_NUM; i++)
    {
        if (sdpage_array[i].pgdir == pgdir)
        {
            sdpage_array[i].user_address = 0;
            sdpage_array[i].block_id = 0;
            sdpage_array[i].pgdir = 0;
            sdpage_array[i].valid = 0;
            used_sdpage--;
        }
    }
    return;
}

uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, uintptr_t kva, int type)
{
    // TODO [P4-task1] alloc_page_helper:
    PTE *pgdir_2 = (PTE *)pgdir;

    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^
                    (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS);
    if ((pgdir_2[vpn2] & _PAGE_PRESENT) == 0)
    {
        // alloc a new second-level page directory
        set_pfn(&pgdir_2[vpn2], kva2pa(allocFixedPage(1, pgdir)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir_2[vpn2], _PAGE_PRESENT | _PAGE_USER);
        // clear_pgdir(pa2kva(get_pa(pgdir_2[vpn2])));
    }
    PTE *pgdir_1 = (PTE *)pa2kva(get_pa(pgdir_2[vpn2]));
    if ((pgdir_1[vpn1] & _PAGE_PRESENT) == 0)
    {
        // alloc a new second-level page directory
        set_pfn(&pgdir_1[vpn1], kva2pa(allocFixedPage(1, pgdir)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir_1[vpn1], _PAGE_PRESENT | _PAGE_USER);
        // clear_pgdir(pa2kva(get_pa(pgdir_1[vpn1])));
    }
    PTE *pgdir_0 = (PTE *)pa2kva(get_pa(pgdir_1[vpn1]));
    if ((pgdir_0[vpn0] & _PAGE_PRESENT) == 0)
    {
        // alloc a new second-level page directory
        if (1 == type)
        {
            set_pfn(&pgdir_0[vpn0], kva2pa(allocPage(1, va, pgdir)) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pgdir_0[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                              _PAGE_EXEC | _PAGE_USER);
        }
        else if (2 == type)
        {
            set_pfn(&pgdir_0[vpn0], kva2pa(kva) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pgdir_0[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                              _PAGE_EXEC | _PAGE_USER);
        }
        else if (3 == type)
        {
            set_pfn(&pgdir_0[vpn0], kva2pa(kva) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pgdir_0[vpn0], _PAGE_PRESENT | _PAGE_READ |
                                              _PAGE_EXEC | _PAGE_USER);
        }
        else
        {
            set_pfn(&pgdir_0[vpn0], kva2pa(allocFixedPage(1, pgdir)) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pgdir_0[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                              _PAGE_EXEC | _PAGE_USER);
        }
        // clear_pgdir(pa2kva(get_pa(pgdir_0[vpn0])));
    }
    uintptr_t p_address = get_pa(pgdir_0[vpn0]) | (va & 0xfff);
    return pa2kva(p_address);
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    uint64_t *dest = (uint64_t *)dest_pgdir;
    uint64_t *src = (uint64_t *)src_pgdir;
    for (int i = 0; i < 512; i++)
    {
        *dest++ = *src++;
    }
}
//更新fifo值
static void fifo_updata()
{
    for (int i = 0; i < PPAGE_NUM; i++)
    {
        if (1 == ppage_array[i].valid)
            ppage_array[i].fifo++;
    }
}
//寻找指定最高的fifo
static int fifo_seek()
{
    for (int i = 100;; i--)
    {
        for (int j = 0; j < PPAGE_NUM; j++)
            if (i == ppage_array[j].fifo)
                return j;
    }
}

//寻找相应的sdpage，然后将物理页的数据转移到sd卡中
#define BLOCK_BEGIN 200
static void p2sd(int num, ptr_t pgdir)
{
    for (int i = 0; i < SDPAGE_NUM; i++)
    {
        if (0 == sdpage_array[i].valid)
        {
            sdpage_array[i].user_address = ppage_array[num].user_address;
            if ((ppage_array[num].kernel_address & 0xfff) != 0)
                assert(0);
            bios_sdwrite(kva2pa(ppage_array[num].kernel_address), 8, BLOCK_BEGIN + 8 * i);
            sdpage_array[i].block_id = BLOCK_BEGIN + 8 * i;
            sdpage_array[i].valid = 1;
            used_sdpage++;
            sdpage_array[i].pgdir = pgdir;
            return;
        }
    }
}

void clear_pagevalid(uint64_t va, ptr_t pgdir)
{
    PTE *pgdir_2 = (PTE *)pgdir;

    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^
                    (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS);
    PTE *pgdir_1 = (PTE *)pa2kva(get_pa(pgdir_2[vpn2]));
    PTE *pgdir_0 = (PTE *)pa2kva(get_pa(pgdir_1[vpn1]));
    set_attribute(&pgdir_0[vpn0], 0);
    return;
}

static void clear_page(uint64_t address)
{
    uint64_t *dst = (uint64_t *)address;
    int len = 512;
    for (; len != 0; len--)
    {
        *dst++ = 0;
    }
    return;
}

static ptr_t shmMemBase = 0x30900000;
shmpage shmpage_array[SHMPAGE_NUM];
int srch_mapping(uint64_t va, ptr_t pgdir)
{
    PTE *pgdir_2 = (PTE *)pgdir;

    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^
                    (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS);
    int p2 = (pgdir_2[vpn2] & _PAGE_PRESENT) == 0;
    if (p2)
        return 0;
    PTE *pgdir_1 = (PTE *)pa2kva(get_pa(pgdir_2[vpn2]));
    int p1 = (pgdir_1[vpn1] & _PAGE_PRESENT) == 0;
    if (p1)
        return 0;
    PTE *pgdir_0 = (PTE *)pa2kva(get_pa(pgdir_1[vpn1]));
    int p0 = (pgdir_0[vpn0] & _PAGE_PRESENT) == 0;
    return !p0;
}
void init_shmpage()
{
    for (int i = 0; i < SHMPAGE_NUM; i++)
    {
        shmpage_array[i].valid = 0;
        shmpage_array[i].key = i;
        shmpage_array[i].pid_num = 0;
        shmpage_array[i].kernel_address = 0;
    }
}
ptr_t do_shmpageget(int key)
{
    int num = key;
    int pid = current_running->pid;
    ptr_t pgdir = current_running->pgdir;
    if (1 == shmpage_array[num].valid)
    {
        shmpage_array[num].pid_num++;
        ptr_t user_address = shmMemBase;
        while (1)
        {
            if (srch_mapping(user_address, pgdir))
                user_address += PAGE_SIZE;
            else
                break;
        }
        shmpage_array[num].user_address[pid] = user_address;
        ptr_t kernel_address = shmpage_array[num].kernel_address;
        ptr_t ret = alloc_page_helper(user_address, pgdir, kernel_address, 2);
        assert(ret == kernel_address);
        return user_address;
    }
    else
    {
        shmpage_array[num].valid = 1;
        shmpage_array[num].pid_num++;
        ptr_t user_address = shmMemBase;
        while (1)
        {
            if (srch_mapping(user_address, pgdir))
                user_address += PAGE_SIZE;
            else
                break;
        }
        shmpage_array[num].user_address[pid] = user_address;
        ptr_t kernel_address = alloc_page_helper(user_address, pgdir, 0, 0);
        shmpage_array[num].kernel_address = kernel_address;
        return user_address;
    }
    assert(0);
}

void do_shmpagedt(void *addr)
{
    ptr_t pgdir = current_running->pgdir;
    int pid = current_running->pid;
    clear_pagevalid(addr, pgdir);
    for (int i = 0; i < SHMPAGE_NUM; i++)
    {
        if (shmpage_array[i].user_address[pid] == addr)
        {
            shmpage_array[i].user_address[pid] = 0;
            shmpage_array[i].pid_num--;
            if (shmpage_array[i].pid_num <= 0)
            {
                shmpage_array[i].valid = 0;
            }
            return;
        }
    }
    assert(0);
}

ptr_t get_page_addr(ptr_t va)
{
    PTE *pgdir_2 = (PTE *)current_running->pgdir;

    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^
                    (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS);
    PTE *pgdir_1 = (PTE *)pa2kva(get_pa(pgdir_2[vpn2]));
    PTE *pgdir_0 = (PTE *)pa2kva(get_pa(pgdir_1[vpn1]));
    ptr_t ret = (pgdir_0[vpn0] >> 10) << 12;
    return ret;
}

ptr_t do_snapget(int key)
{
    int num = key;
    int pid = current_running->pid;
    ptr_t pgdir = current_running->pgdir;
    if (1 == shmpage_array[num].valid)
    {
        shmpage_array[num].pid_num++;
        ptr_t user_address = shmMemBase;
        while (1)
        {
            if (srch_mapping(user_address, pgdir))
                user_address += PAGE_SIZE;
            else
                break;
        }
        shmpage_array[num].user_address[pid] = user_address;
        ptr_t kernel_address = shmpage_array[num].kernel_address;
        ptr_t ret = alloc_page_helper(user_address, pgdir, kernel_address, 3);
        assert(ret == kernel_address);
        return user_address;
    }
    else
    {
        shmpage_array[num].valid = 1;
        shmpage_array[num].pid_num++;
        ptr_t user_address = shmMemBase;
        while (1)
        {
            if (srch_mapping(user_address, pgdir))
                user_address += PAGE_SIZE;
            else
                break;
        }
        shmpage_array[num].user_address[pid] = user_address;
        ptr_t kernel_address = alloc_page_helper(user_address, pgdir, 0, 0);
        shmpage_array[num].kernel_address = kernel_address;
        return user_address;
    }
    assert(0);
}
