#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR
#define KVA_PA_OFFSET 0xffffffc000000000lu

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)
#define ATTRIBUTE_MASK ((1lu << 9) - 1)

#define KERNEL_VA_OFFSET 0xffffffc000000000

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

//for get_kva_of
#define VPN2_SHIFT 28
#define VPN1_SHIFT 19
#define VPN0_SHIFT 10

#define PPN_MASK ((1lu << PPN_BITS) - 1)
#define VPN_MASK ((1lu << PPN_BITS) - 1)

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    /* TODO: [P4-task1] */
    /*
    uintptr_t pa = kva - KERNEL_VA_OFFSET;
    return pa;
    */
    uintptr_t mask = 0xffffffff;
    uintptr_t pa = kva & mask;
    return pa;
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    /* TODO: [P4-task1] */
    /*
    uintptr_t kva = pa + KERNEL_VA_OFFSET;
    return kva;
    */
    uintptr_t mask = 0xffffffc000000000;
    uintptr_t kva = pa | mask;
    return kva;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    /* TODO: [P4-task1] */
    uint64_t pa = ((entry >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT);
    return pa;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    uint64_t pfn = (entry >> _PAGE_PFN_SHIFT);
    return pfn;
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    //step1：清空其属性
    *entry = *entry & ATTRIBUTE_MASK;
    //step2: 设置其pfn
    //(pfn << _PAGE_PFN_SHIFT)是令其左移10位与页表项对应pfn对齐
    *entry = *entry | (pfn << _PAGE_PFN_SHIFT);
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    /* TODO: [P4-task1] */
    uint64_t attribute = entry & mask;  //返回页表项的属性对应位
    return attribute;
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    //*entry = *entry | bits;     //将页表项的属性对应位置1
    uint64_t tmp = *entry & ~0x3ff;
    *entry = tmp | bits;
}

static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    PTE *early_pgdir = (PTE *)pgdir_addr;

    for(int i=0; i<512; i++)
    {
        early_pgdir[i] = 0;
    }
}

/* 
 * query the page table stored in pgdir_va to obtain the physical 
 * address corresponding to the virtual address va.
 * 
 * return the kernel virtual address of the physical address 
 */
static inline uintptr_t get_kva_of(uintptr_t va, uintptr_t pgdir_va)
{
    // TODO: [P4-task1] (todo if you need)
    unsigned int VPN2 = (va >> VPN2_SHIFT) & VPN_MASK;
    unsigned int VPN1 = (va >> VPN1_SHIFT) & VPN_MASK;
    unsigned int VPN0 = (va >> VPN0_SHIFT) & VPN_MASK;

    PTE* first_pgdir_va = (PTE *)pgdir_va;

    PTE* second_pgdir_pa = (PTE *)get_pa(first_pgdir_va[VPN2]);
    PTE* second_pgdir_va = (PTE *)pa2kva(second_pgdir_pa);

    PTE* third_pgdir_pa = (PTE *)get_pa(second_pgdir_va[VPN1]);
    PTE* third_pgdir_va = (PTE *)pa2kva(third_pgdir_pa);

    uintptr_t pa = get_pa(third_pgdir_va[VPN0]) | (va & NORMAL_PAGE_SHIFT);
    return pa2kva(pa);
}

static inline void clear_ker_map(uint64_t va)
{
    unsigned int VPN2 = (va >> VPN2_SHIFT) & VPN_MASK;
    unsigned int VPN1 = (va >> VPN1_SHIFT) & VPN_MASK;
    
    PTE* first_pgdir_pa = (PTE*)PGDIR_PA;
    PTE* first_pgdir_va = (PTE*)pa2kva(first_pgdir_pa);

    PTE* second_pgdir_pa = (PTE*)get_pa(first_pgdir_va[VPN2]);
    PTE* second_pgdir_va = (PTE*)pa2kva(second_pgdir_pa);
    
    second_pgdir_va[VPN1] = 0;
    local_flush_tlb_page(va);
    
    return;

}

static inline void clear_temporary_map()
{
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu;
         va += 0x200000lu) {
        clear_ker_map(va);
    }
    return;
}


#endif  // PGTABLE_H
