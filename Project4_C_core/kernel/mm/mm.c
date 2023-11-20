#include <os/mm.h>
#include <pgtable.h>
#include <os/sched.h>
#include <os/kernel.h>

static ptr_t pgdirMemCurr = FREEMEM_KERNEL;
static ptr_t kernMemCurr = KERNMEM_BASE;
static ptr_t userMemCurr = USERMEM_BASE;

static ptr_t stuMemCurr = STUMEM_KERNEL;
static ptr_t shaMemCurr = SHAREMEM_BASE;
static ptr_t shauserCurr = SHAREMEM_USER_BASE;
/*
ptr_t Page_Addr[MAX_PAGE_NUM]={0};
int Page_Valid[MAX_PAGE_NUM]={0}; 
*/
pgdir pgdir_mm[MAX_PAGE_NUM];
pgdir kpages_mm[MAX_KPAGE_NUM];
page pages_mm[MAX_STU_PAGE_NUM];
sdpage sdpages_mm[MAX_SD_PAGE_NUM];
shpage shpages_mm[MAX_SH_PAGE_NUM];

void init_mm(void)
{
    for(int i=0; i < MAX_PAGE_NUM; i++)
    {
        pgdir_mm[i].page_addr = pgdirMemCurr;
        pgdir_mm[i].page_valid = FREE;
        pgdirMemCurr += PAGE_SIZE;
    }

    for(int i=0; i < MAX_KPAGE_NUM; i++)
    {
        kpages_mm[i].page_addr = kernMemCurr;
        kpages_mm[i].page_valid = FREE;
        kernMemCurr += PAGE_SIZE;
    }

    for(int i=0; i < MAX_STU_PAGE_NUM; i++)
    {
        pages_mm[i].page_addr = stuMemCurr;
        pages_mm[i].page_valid = FREE;
        pages_mm[i].user_addr = 0;
        pages_mm[i].pgdir = 0;
        stuMemCurr += PAGE_SIZE;
    }

    for(int i=0; i < MAX_SD_PAGE_NUM; i++)
    {
        sdpages_mm[i].block_index = 0;
        sdpages_mm[i].page_valid = FREE;
        sdpages_mm[i].round_user_addr = 0;
    }

    for(int i=0; i < MAX_SH_PAGE_NUM; i++)
    {
        shpages_mm[i].page_addr = shaMemCurr;
        shpages_mm[i].share_num = FREE;
        shaMemCurr += PAGE_SIZE;
    }
}

ptr_t allocPgdir(int numPage, pcb_t *ptr_to_pcb)
{
    for(int i=0; i<MAX_PAGE_NUM; i++)
    {
        if(pgdir_mm[i].page_valid == FREE)
        {
            pgdir_mm[i].page_valid = USED;
            ptr_to_pcb->page_addr[ptr_to_pcb->num_of_page] = i;
            ptr_to_pcb->kind[ptr_to_pcb->num_of_page] = 0;
            ptr_to_pcb->num_of_page++;

            clear_pgdir(pgdir_mm[i].page_addr);
            return pgdir_mm[i].page_addr;
        }
    }
    return 0;
}

ptr_t allocKernPage(int numPage, pcb_t *ptr_to_pcb)
{
    for(int i=0; i<MAX_KPAGE_NUM; i++)
    {
        if(kpages_mm[i].page_valid == FREE)
        {
            kpages_mm[i].page_valid = USED;
            ptr_to_pcb->page_addr[ptr_to_pcb->num_of_page] = i;
            ptr_to_pcb->kind[ptr_to_pcb->num_of_page] = 1;
            ptr_to_pcb->num_of_page++;
            return kpages_mm[i].page_addr;
        }
    }
    return 0;
}

ptr_t allocPage(int numPage, pcb_t *ptr_to_pcb, ptr_t vaddr)
{
    for(int i=0; i < MAX_STU_PAGE_NUM; i++)
    {
        if(pages_mm[i].page_valid == FREE)
        {
            //将该页的valid置1
            pages_mm[i].page_valid = USED;
            pages_mm[i].user_addr = get_base_addr(vaddr);
            pages_mm[i].pgdir = ptr_to_pcb->pgdir;
            pages_mm[i].used_time = 1;
            //记录该页的地址
            //将进程pcb的num_of_page增1
            ptr_to_pcb->page_addr[ptr_to_pcb->num_of_page] = i;
            ptr_to_pcb->kind[ptr_to_pcb->num_of_page] = 2;
            ptr_to_pcb->num_of_page++;

            //更新表中所有页的使用时间
            time_update();

            return pages_mm[i].page_addr;
        }
    }
    
    //otherwise, we should substituate page (no free page)
    //printl("no free page , must exchange!\n");

    int page_index = choose_ex_page();
    //根据被使用时间选定要被替换的页
    //printl("exchange page %d, origin address is %x\n",page_index, pages_mm[page_index].user_addr);

    //bios_sdwrite(kva2pa(pages_mm[page_index].page_addr), 8, exchange_block_index);
    //将被替换页写入sd卡，并在sdpage结构体中存储其信息
    write_sd(page_index, pages_mm[page_index].pgdir);

    //使得原映射无效,以确保下次访问时发生缺页中断。
    //printl("ori_addr: %x, new_pgdir: %x\n", pages_mm[page_index].user_addr, vaddr);
    make_page_invalid(pages_mm[page_index].user_addr, pages_mm[page_index].pgdir);

    //pages_mm[page_index].page_valid = FREE;
    //return allocPage(1, ptr_to_pcb, vaddr);

    //确保有效位
    pages_mm[page_index].page_valid = USED;
    //重置user_address
    pages_mm[page_index].user_addr = get_base_addr(vaddr);
    //置pgdir
    pages_mm[page_index].pgdir = ptr_to_pcb->pgdir;
    //将该页的占用时间重置
    pages_mm[page_index].used_time = 1;
    //更新表中所有页的使用时间
    time_update();

    for(int i=0; i<ptr_to_pcb->num_of_page; i++)
    {
        if(page_index == ptr_to_pcb->page_addr[i] && 
            (ptr_to_pcb->kind[i] == 3 || ptr_to_pcb->kind[i] == 2))
        {
            ptr_to_pcb->kind[i] = 2;
            return pages_mm[page_index].page_addr;
        }
    }

    ptr_to_pcb->page_addr[ptr_to_pcb->num_of_page] = page_index;
    ptr_to_pcb->kind[ptr_to_pcb->num_of_page] = 2;
    ptr_to_pcb->num_of_page++;

    return pages_mm[page_index].page_addr;

    /*
    if(ptr_to_pcb->pgdir != pages_mm[page_index].pgdir)     //若被替换页本身就属于该进程，则没必要重新加入
    {
        ptr_to_pcb->page_addr[ptr_to_pcb->num_of_page] = page_index;
        ptr_to_pcb->kind[ptr_to_pcb->num_of_page] = 1;
        ptr_to_pcb->num_of_page++;
    }
    else
    {
        printl("same pcb, page_index is %d\n", page_index);
        for(int i=0; i<ptr_to_pcb->num_of_page; i++)
        {
            printl("%d\n",ptr_to_pcb->page_addr[i]);
            if((ptr_to_pcb->page_addr[i] == page_index) && (ptr_to_pcb->kind[i] == 2))
            {
                ptr_to_pcb->kind[i] = 1;
                break;
            }
        }
    }
    */
}

void freePage(pcb_t *pcb)
{
    int page_index;
    printl("this process has %d page in total\n\n",pcb->num_of_page);
    for(int i=0; i<pcb->num_of_page; i++)
    {
        //printl("page %d:",i);
        if(pcb->kind[i] == 0)
        {
            //页目录
            printl("free pgdir\n");
            page_index = pcb->page_addr[i];
            printl("page_index is %d\n",page_index);
            pgdir_mm[page_index].page_valid = FREE;
        }
        else if(pcb->kind[i] == 1)
        {
            //具体物理页
            //printl("free kernpage\n");
            page_index = pcb->page_addr[i];
            //printl("kernpage_index is %d\n",page_index);
            kpages_mm[page_index].page_valid = FREE;
        }
        else if(pcb->kind[i] == 2)
        {
            printl("free p_page\n");
            page_index = pcb->page_addr[i];
            printl("ppage_index is %d\n",page_index);
            pages_mm[page_index].page_valid = FREE;
        }
        else
        {
            printl("cancel page, can't recycle now\n");
        }
    }

    
    for(int i=0; i<MAX_SD_PAGE_NUM; i++)
    {
        if(sdpages_mm[i].pgdir == current_running->pgdir)
        {
            printl("free sdpages %d\n", i);
            sdpages_mm[i].page_valid = FREE;
        }
    }
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    memcpy((uint8_t *)dest_pgdir, (uint8_t *)src_pgdir, PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb, bool still)
{
    // TODO [P4-task1] alloc_page_helper:
    /*
    *param:
    *va: 虚拟地址va
    *pgdir：三级页表的页表目录
    *ptr_to_pcb：指向当前进程pcb的指针
    *return：最终物理页的虚拟地址
    */

    va &= VA_MASK;   //VA_MASK = 39 * 1
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    //printl("vpn2 is %d\n",vpn2);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    //printl("vpn1 is %d\n",vpn1);
    /*
     -------------------------------------------------
    |     vpn2      |     vpn1      |      vpn0      |       va >> NORMAL_PAGE_SHIFT
    -------------------------------------------------                   ^
    |     vpn2      |                                        vpn2 << (PPN_BITS + PPN_BITS)
    --------------------------------                                    ^
                    |     vpn1     |                         vpn1 << PPN_BITS
                    ----------------                                    =
    -->                                                                vpn0
    */
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 
    //printl("vpn0 is %d\n",vpn0);  
    
    //一级页表，零级页表的页表虚拟地址和物理页的虚拟地址
    ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
    //一级页表，零级页表的页表物理地址和物理页的物理地址
    ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;
    ptr_t if_valid1, if_valid0;

    //attribute
    /*
    _PAGE_PRESENT：有效位
    _PAGE_USER：User位，当U为0时，该页面在User-mode下访问会触发缺页异常
    当U为1时，该页面仅在User-mode下也可访问。

    _PAGE_READ: 可读                    \
    _PAGE_WRITE: 可写                    |
    _PAGE_EXEC: 可执行                   |
    _PAGE_ACCESSED: 是否被访问           |
    注意为0表示已被访问，所以这里要置1     -->for 零级页表   
    _PAGE_DIRTY: 是否被写过             |
    注意为0表示已被写，所以这里要置1     /
    */
    ptr_t attribute_bit_for_path = _PAGE_PRESENT | _PAGE_USER;
    ptr_t attribute_bit_for_target = _PAGE_PRESENT | _PAGE_USER | _PAGE_READ |
                                     _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED |
                                     _PAGE_DIRTY;

    //根据vpn2索引二级页表
    ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
    //该表项是否有效
    ptr_t if_valid2 = pgdir2_content & _PAGE_PRESENT;

    if(if_valid2 != 0)
    {
        pgdir1_vpn = pa2kva(get_pa(pgdir2_content));
        //printl("3 valid! its vpn is %x\n", pgdir1_vpn);
    }
    else
    {
        //若该表项无效，则需创建对应1级页表并填充2级页表表项对应内容
        //step1：创建1级页表
        pgdir1_vpn = allocPgdir(1, ptr_to_pcb);
        //printl("3 invalid! allocpage %x\n", pgdir1_vpn);
        //pgdir1 为新分配页表的虚拟地址
        //现在要填充该1级页表项的具体内容
        //step2-1：填充ppn
        /*
        (kva2pa(pgdir1_vpn): ppn2 ppn1 ppn0 page_shift(12)
                                                |
                                                |
                                                V
                                            ppn2 ppn1 ppn0
        */
        pgdir1_ppn = (kva2pa(pgdir1_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir+vpn2, pgdir1_ppn);
        //step2-2：填充attribute
        set_attribute((PTE *)pgdir+vpn2, attribute_bit_for_path);
    }

    //根据vpn1索引一级页表
    ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
    //该表项是否有效
    if_valid1 = pgdir1_content & _PAGE_PRESENT;

    if(if_valid1 != 0)
    {
        pgdir0_vpn = pa2kva(get_pa(pgdir1_content));
        //printl("2 valid! its vpn is %x\n", pgdir0_vpn);
    }
    else
    {
        pgdir0_vpn = allocPgdir(1, ptr_to_pcb);
        //printl("2 invalid! allocpage %x\n", pgdir0_vpn);
        pgdir0_ppn = (kva2pa(pgdir0_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir1_vpn+vpn1, pgdir0_ppn);
        set_attribute((PTE *)pgdir1_vpn+vpn1, attribute_bit_for_path);
    }

    //根据vpn0索引一级页表
    ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
    //该表项是否有效
    if_valid0 = pgdir0_content & _PAGE_PRESENT;

    if(if_valid0 != 0)
    {
        pg_vpn = pa2kva(get_pa(pgdir0_content));
        //printl("1 valid! its vpn is %x\n", pg_vpn);
    }
    else
    {
        if(still)
            pg_vpn = allocPgdir(1, ptr_to_pcb);
        else
            pg_vpn = allocPage(1, ptr_to_pcb, va);

        //printl("1 invalid! allocpage %x\n", pg_vpn);
        pg_ppn = (kva2pa(pg_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir0_vpn+vpn0, pg_ppn);
        /*
        对零级页表，attribute设置需要增加read,write,exe,accessed,dirty置1
        */
        set_attribute((PTE *)pgdir0_vpn+vpn0, attribute_bit_for_target);
    }

    //printl("vpn2 = %d, vpn1 = %d, vpn0 = %d\n",vpn2,vpn1,vpn0);
    //返回最终物理页的虚拟地址
    return pg_vpn;
}

uintptr_t alloc_snappage_helper(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb)
{
    va &= VA_MASK;   //VA_MASK = 39 * 1
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 

    ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
    ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;
    ptr_t if_valid1, if_valid0;

    ptr_t attribute_bit_for_path = _PAGE_PRESENT | _PAGE_USER;
    ptr_t attribute_bit_for_target = _PAGE_PRESENT | _PAGE_USER | _PAGE_READ |
                                     _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY;

    ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
    ptr_t if_valid2 = pgdir2_content & _PAGE_PRESENT;

    if(if_valid2 != 0)
    {
        pgdir1_vpn = pa2kva(get_pa(pgdir2_content));
    }
    else
    {
        pgdir1_vpn = allocPgdir(1, ptr_to_pcb);
        pgdir1_ppn = (kva2pa(pgdir1_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir+vpn2, pgdir1_ppn);
        set_attribute((PTE *)pgdir+vpn2, attribute_bit_for_path);
    }

    ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
    if_valid1 = pgdir1_content & _PAGE_PRESENT;

    if(if_valid1 != 0)
    {
        pgdir0_vpn = pa2kva(get_pa(pgdir1_content));
    }
    else
    {
        pgdir0_vpn = allocPgdir(1, ptr_to_pcb);
        pgdir0_ppn = (kva2pa(pgdir0_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir1_vpn+vpn1, pgdir0_ppn);
        set_attribute((PTE *)pgdir1_vpn+vpn1, attribute_bit_for_path);
    }

    ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
    if_valid0 = pgdir0_content & _PAGE_PRESENT;

    if(if_valid0 != 0)
    {
        pg_vpn = pa2kva(get_pa(pgdir0_content));
    }
    else
    {
        pg_vpn = allocPage(1, ptr_to_pcb, va);

        pg_ppn = (kva2pa(pg_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir0_vpn+vpn0, pg_ppn);
        set_attribute((PTE *)pgdir0_vpn+vpn0, attribute_bit_for_target);
    }

    return pg_vpn;
}

ptr_t make_page_invalid(uintptr_t va, uintptr_t pgdir)
{
    va &= VA_MASK;   //VA_MASK = 39 * 1
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 

    ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
    ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;

    ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
    pgdir1_vpn = pa2kva(get_pa(pgdir2_content));

    ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
    pgdir0_vpn = pa2kva(get_pa(pgdir1_content));

    ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
    ptr_t attribute_invalid =  0;
    pg_ppn = get_pfn((PTE *)pgdir0_vpn+vpn0);

    set_attribute((PTE *)pgdir0_vpn+vpn0, attribute_invalid);
    return pg_ppn;
    //printl("in invalid, vpn2 = %d, vpn1 = %d, vpn0 = %d\n",vpn2,vpn1,vpn0);
}

void make_page_cantwrite(uintptr_t va, uintptr_t pgdir)
{
    va &= VA_MASK;   //VA_MASK = 39 * 1
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 

    ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
    ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;

    ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
    pgdir1_vpn = pa2kva(get_pa(pgdir2_content));

    ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
    pgdir0_vpn = pa2kva(get_pa(pgdir1_content));

    ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
    ptr_t attribute_cantwrite = _PAGE_PRESENT | _PAGE_USER | _PAGE_READ |
                                    _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY;
    pg_ppn = get_pfn((PTE *)pgdir0_vpn+vpn0);

    set_attribute((PTE *)pgdir0_vpn+vpn0, attribute_cantwrite);
    return pg_ppn;
    //printl("in invalid, vpn2 = %d, vpn1 = %d, vpn0 = %d\n",vpn2,vpn1,vpn0);
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    if(key < 0)
        return 0;
    int sh_index = key & MAX_SH_PAGE_NUM;
    
    shpages_mm[sh_index].share_num += 1;
    //shauserCurr += PAGE_SIZE;
    ptr_t vaddr = find_valid_addr(current_running->pgdir);
    alloc_share_page(vaddr, current_running->pgdir, current_running,
                     shpages_mm[sh_index].page_addr);

    //printl("return vaddr %x\n", vaddr);
    return vaddr;
}

uintptr_t snp_page_get(int key)
{
    if(key < 0)
        return 0;
    int sh_index = key & MAX_SH_PAGE_NUM;
    
    shpages_mm[sh_index].share_num += 1;
    //shauserCurr += PAGE_SIZE;
    ptr_t vaddr = find_valid_addr(current_running->pgdir);
    if(shpages_mm[sh_index].share_num == 1)
    {
        shpages_mm[sh_index].origin_va = vaddr;
        alloc_snp_page(vaddr, current_running->pgdir, current_running,
                shpages_mm[sh_index].page_addr);
    }
    else
    {
        alloc_snp_page(vaddr, current_running->pgdir, current_running,
                pa2kva(get_page_addr(shpages_mm[sh_index].origin_va)));
    }

    make_page_cantwrite(shpages_mm[sh_index].origin_va, current_running->pgdir);

    //printk("return vaddr %x\n", vaddr);
    return vaddr;
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    ptr_t pg_ppn = make_page_invalid(addr, current_running->pgdir);
    for(int i=0; i < MAX_SH_PAGE_NUM; i++)
    {
        if(shpages_mm[i].page_addr == pa2kva(pg_ppn))
        {
            shpages_mm[i].share_num--;
            break;
        }
    }
}


void time_update()
{
    for(int i=0; i<MAX_STU_PAGE_NUM; i++)
    {
        if(pages_mm[i].page_valid == USED)
            //该页面被使用的时间+1
            pages_mm[i].used_time++;
    }
}

int choose_ex_page()
{
    int index;
    int max_time = 0;
    for(int i=0; i<MAX_STU_PAGE_NUM; i++)
    {
        if(pages_mm[i].used_time > max_time)
        {
            max_time = pages_mm[i].used_time;
            index = i;
        }
    }
    return index;
}

void write_sd(int page_index, ptr_t pgdir)
{
    for(int i=0; i < MAX_SD_PAGE_NUM; i++)
    {
        if(sdpages_mm[i].page_valid == FREE)
        {
            sdpages_mm[i].page_valid = USED;
            sdpages_mm[i].pages_mm_index = page_index;
            ptr_t round_addr = get_base_addr(pages_mm[page_index].user_addr);
            sdpages_mm[i].round_user_addr = round_addr;
            sdpages_mm[i].pgdir = pgdir;
            bios_sdwrite(kva2pa(pages_mm[page_index].page_addr), 8, EXCHANGE_BLOCK_INDEX + 8 * i);
            sdpages_mm[i].block_index = EXCHANGE_BLOCK_INDEX + 8 * i;
            return;
        }
    }
    printl("no sd space available\n");
}

uint64_t get_base_addr(uint64_t addr)
{
    return ((addr >> 12) << 12);
}

void alloc_share_page(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb, ptr_t page_addr)
{
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 

    ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
    ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;
    ptr_t if_valid1, if_valid0;

    ptr_t attribute_bit_for_path = _PAGE_PRESENT | _PAGE_USER;
    ptr_t attribute_bit_for_target = _PAGE_PRESENT | _PAGE_USER | _PAGE_READ |
                                     _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED |
                                     _PAGE_DIRTY;

    ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
    ptr_t if_valid2 = pgdir2_content & _PAGE_PRESENT;

    if(if_valid2 != 0)
    {
        pgdir1_vpn = pa2kva(get_pa(pgdir2_content));
    }
    else
    {
        pgdir1_vpn = allocPgdir(1, ptr_to_pcb);
        pgdir1_ppn = (kva2pa(pgdir1_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir+vpn2, pgdir1_ppn);
        set_attribute((PTE *)pgdir+vpn2, attribute_bit_for_path);
    }

    ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
    if_valid1 = pgdir1_content & _PAGE_PRESENT;

    if(if_valid1 != 0)
    {
        pgdir0_vpn = pa2kva(get_pa(pgdir1_content));
    }
    else
    {
        pgdir0_vpn = allocPgdir(1, ptr_to_pcb);
        pgdir0_ppn = (kva2pa(pgdir0_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir1_vpn+vpn1, pgdir0_ppn);
        set_attribute((PTE *)pgdir1_vpn+vpn1, attribute_bit_for_path);
    }   

    ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
    if_valid0 = pgdir0_content & _PAGE_PRESENT;

    if(if_valid0 != 0)
    {
        pg_vpn = pa2kva(get_pa(pgdir0_content));
    }
    else
    {
        pg_vpn = page_addr;

        //printl("1 invalid! allocpage %x\n", pg_vpn);
        pg_ppn = (kva2pa(pg_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir0_vpn+vpn0, pg_ppn);
        set_attribute((PTE *)pgdir0_vpn+vpn0, attribute_bit_for_target);
    }
}

void alloc_snp_page(uintptr_t va, uintptr_t pgdir, pcb_t *ptr_to_pcb, ptr_t page_addr)
{
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 

    ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
    ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;
    ptr_t if_valid1, if_valid0;

    ptr_t attribute_bit_for_path = _PAGE_PRESENT | _PAGE_USER;
    ptr_t attribute_bit_for_target = _PAGE_PRESENT | _PAGE_USER | _PAGE_READ |
                                     _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY;

    ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
    ptr_t if_valid2 = pgdir2_content & _PAGE_PRESENT;

    if(if_valid2 != 0)
    {
        pgdir1_vpn = pa2kva(get_pa(pgdir2_content));
    }
    else
    {
        pgdir1_vpn = allocPgdir(1, ptr_to_pcb);
        pgdir1_ppn = (kva2pa(pgdir1_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir+vpn2, pgdir1_ppn);
        set_attribute((PTE *)pgdir+vpn2, attribute_bit_for_path);
    }

    ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
    if_valid1 = pgdir1_content & _PAGE_PRESENT;

    if(if_valid1 != 0)
    {
        pgdir0_vpn = pa2kva(get_pa(pgdir1_content));
    }
    else
    {
        pgdir0_vpn = allocPgdir(1, ptr_to_pcb);
        pgdir0_ppn = (kva2pa(pgdir0_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir1_vpn+vpn1, pgdir0_ppn);
        set_attribute((PTE *)pgdir1_vpn+vpn1, attribute_bit_for_path);
    }   

    ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
    if_valid0 = pgdir0_content & _PAGE_PRESENT;

    if(if_valid0 != 0)
    {
        pg_vpn = pa2kva(get_pa(pgdir0_content));
    }
    else
    {
        pg_vpn = page_addr;

        //printl("1 invalid! allocpage %x\n", pg_vpn);
        pg_ppn = (kva2pa(pg_vpn) >> NORMAL_PAGE_SHIFT);
        set_pfn((PTE *)pgdir0_vpn+vpn0, pg_ppn);
        set_attribute((PTE *)pgdir0_vpn+vpn0, attribute_bit_for_target);
    }
}

ptr_t find_valid_addr(uintptr_t pgdir)
{
    ptr_t va;
    for(va = shauserCurr; va < (shauserCurr + 256 * PAGE_SIZE); va += PAGE_SIZE)
    {
        va &= VA_MASK;
        uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 
    
        ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
        ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;
        ptr_t if_valid1, if_valid0;

        ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
        ptr_t if_valid2 = pgdir2_content & _PAGE_PRESENT;

        if(if_valid2 == 0)
            return va;
        else
            pgdir1_vpn = pa2kva(get_pa(pgdir2_content));

        ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
        if_valid1 = pgdir1_content & _PAGE_PRESENT;

        if(if_valid1 == 0)
            return va;
        else
            pgdir0_vpn = pa2kva(get_pa(pgdir1_content));

        ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
        if_valid0 = pgdir0_content & _PAGE_PRESENT;

        if(if_valid0 == 0)
            return va;
    }
    return 0;
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

bool check_if_snp(uintptr_t va, uintptr_t pgdir)
{
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (PPN_BITS + PPN_BITS)) ^ (vpn1 << PPN_BITS); 
    
    ptr_t pgdir1_vpn, pgdir0_vpn, pg_vpn;
    ptr_t pgdir1_ppn, pgdir0_ppn, pg_ppn;
    ptr_t if_valid1, if_valid0;
    ptr_t if_can_write;

    ptr_t pgdir2_content = ((PTE *)pgdir)[vpn2];
    ptr_t if_valid2 = pgdir2_content & _PAGE_PRESENT;

    if(if_valid2 == 0)
        return FALSE;
    else
        pgdir1_vpn = pa2kva(get_pa(pgdir2_content));

    ptr_t pgdir1_content = ((PTE *)pgdir1_vpn)[vpn1];
    if_valid1 = pgdir1_content & _PAGE_PRESENT;

    if(if_valid1 == 0)
        return FALSE;
    else
        pgdir0_vpn = pa2kva(get_pa(pgdir1_content));

    ptr_t pgdir0_content = ((PTE *)pgdir0_vpn)[vpn0];
    if_valid0 = pgdir0_content & _PAGE_PRESENT;
    if_can_write = pgdir0_content & _PAGE_WRITE;

    if(if_valid0 == 1 && if_can_write == 0)
        return TRUE;
    else
        return FALSE;
}

void make_page_valid(uintptr_t va, uintptr_t pgdir)
{
    va &= VA_MASK;
    PTE *pgdir_2 = (PTE *)pgdir;
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