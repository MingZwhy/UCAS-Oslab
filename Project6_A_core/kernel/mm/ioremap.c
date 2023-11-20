#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

#define H_PAGE_SIZE 0x200000lu
#define KERNEL_PAGE 0xffffffc059000000

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;
uintptr_t io_kaddr = IO_ADDR_START;
static uintptr_t pg_base = KERNEL_PAGE;
static uintptr_t alloc_page_v()
{
    //每页4KB 0x1000
    /*
    PGDIR_PA --> PGDIR_PA + 0x1000 为唯一的三级页表
    从PGDIR_PA + 0x1000开始分配二级页表...一级页表
    */
    pg_base += 0x1000;
    return pg_base;
}

static void map_page_v(uint64_t va, uint64_t pa, PTE *pgdir)
{
    uint64_t vpn2,vpn1;
    va &= VA_MASK;
    vpn2 =  va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    vpn1 =  (vpn2 << PPN_BITS) ^
            (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));

    PTE* first_pgdir = (PTE*)pgdir;
    if(first_pgdir[vpn2] == 0){
        //与boot.c里不同的是这里要用虚址，因为已经开启了虚存。
        set_pfn(&first_pgdir[vpn2], kva2pa(alloc_page_v()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&first_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(first_pgdir[vpn2])));
    }

    PTE* second_pgdir = (PTE*)pa2kva(get_pa(first_pgdir[vpn2]));
    set_pfn(&second_pgdir[vpn1], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
    &second_pgdir[vpn1], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                    _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);

}

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    
    /*
    uint64_t offset = phys_addr & 0xfff;
    unsigned long p_addr = phys_addr - offset;
    unsigned long psize = size + offset;
    int num_of_page = psize / H_PAGE_SIZE + (psize % H_PAGE_SIZE != 0);

    uintptr_t first_addr = io_kaddr;
    PTE *early_pgdir = (PTE *)PGDIR_PA;

    for(int i=0; i<num_of_page; i++)
    {
        io_kaddr = io_kaddr + i * H_PAGE_SIZE;
        uintptr_t pa = (uintptr_t)(p_addr + i * H_PAGE_SIZE);
        map_page(io_kaddr, pa, early_pgdir);
    }

    local_flush_tlb_all();
    return (first_addr + offset);
    */
   if(size < H_PAGE_SIZE)
   {
        size = H_PAGE_SIZE;
   }
    void* ret = io_base;
    uint64_t kva = ret;
    uint64_t pa = phys_addr;

    io_base += size;
    //注意这里pgdir要用虚址
    PTE *pgdir = (PTE *)(PGDIR_PA + 0xffffffc000000000);

    for(; kva < io_base; kva += H_PAGE_SIZE, pa += H_PAGE_SIZE)
    {
        map_page_v(kva, pa, pgdir);
    }
    local_flush_tlb_all();
    return ret;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}

