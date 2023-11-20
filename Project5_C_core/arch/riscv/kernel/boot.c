/* RISC-V kernel boot stage */
#include <pgtable.h>
#include <asm.h>

typedef void (*kernel_entry_t)(unsigned long);

/********* setup memory mapping ***********/
static uintptr_t alloc_page()
{
    //每页4KB 0x1000
    /*
    PGDIR_PA --> PGDIR_PA + 0x1000 为唯一的三级页表
    从PGDIR_PA + 0x1000开始分配二级页表...一级页表
    */
    static uintptr_t pg_base = PGDIR_PA;
    pg_base += 0x1000;
    return pg_base;
}

// using 2MB large page
static void map_page(uint64_t va, uint64_t pa, PTE *pgdir)
{
    va &= VA_MASK;   //VA_MASK = 39 * 1
    //将va右移12位的页内偏移shift，再右移9位的PPN[0]，右移9位的PPN[1]得到PPN[2]
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    if (pgdir[vpn2] == 0) {
        // alloc a new second-level page directory
        //为新的二级页表分配了一个物理页框，并将该物理页框的地址存入了表项之中
        set_pfn(&pgdir[vpn2], alloc_page() >> NORMAL_PAGE_SHIFT);   //置pfn
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);                 //置有效位
        clear_pgdir(get_pa(pgdir[vpn2]));                           //清空该页表
    }
    PTE *pmd = (PTE *)get_pa(pgdir[vpn2]);
    /*
    因为这里是一个2MB的大页表，所以其二级页表项中记录的并非三级页表所在物理页号，
    而是直接是最终映射的物理地址的物理页号，
    因此set_pfn的第二个参数不再是为页表新分配的物理页框，而是直接是最终映射的物理地址的物理页号
    */
    set_pfn(&pmd[vpn1], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &pmd[vpn1], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);
}

static void enable_vm()
{
    // write satp to enable paging
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
}

/* Sv-39 mode
 * 0x0000_0000_0000_0000-0x0000_003f_ffff_ffff is for user mode
 * 0xffff_ffc0_0000_0000-0xffff_ffff_ffff_ffff is for kernel mode
 */
static void setup_vm()
{
    clear_pgdir(PGDIR_PA);
    // map kernel virtual address(kva) to kernel physical
    // address(kpa) kva = kpa + 0xffff_ffc0_0000_0000 use 2MB page,
    // map all physical memory
    PTE *early_pgdir = (PTE *)PGDIR_PA;

    /*
    可用空间一共256MB
    0xffffffc050000000lu --> 0xffffffc060000000lu
    2MB页 --> 200000lu 
    */

    for (uint64_t kva = 0xffffffc050000000lu;
         kva < 0xffffffc060000000lu; kva += 0x200000lu) {
        map_page(kva, kva2pa(kva), early_pgdir);
    }
    // map boot address
    for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu;
         pa += 0x200000lu) {
        map_page(pa, pa, early_pgdir);
    }
    enable_vm();
}

extern uintptr_t _start[];

/*********** start here **************/
int boot_kernel(unsigned long mhartid)
{
    if (mhartid == 0) {
        setup_vm();
    } else {
        enable_vm();
    }
    //while(1);

    /* go to kernel */
    //clear_temporary_map();

    ((kernel_entry_t)pa2kva(_start))(mhartid);

    return 0;
}
