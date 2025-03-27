/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <stdint.h>
#include <kafka/types.hpp>
#include <kafka/pmem.hpp>
#include <kafka/X86cpu.hpp>
#include <kafka/X86vmem.hpp>
#include <kafka/hal/cpu.hpp>
#include <kafka/hal/vmem.hpp>

namespace kfk
{
    namespace 
    {
        void* memset(void *s, const int c, size_t count)
        {
            auto *xs = static_cast<uint8_t *>(s);
            const auto b = static_cast<uint8_t>(c);
            if (count < 8)
            {
                while (count--)
                    *xs++ = b;
                return s;
            }
    
            /* align to word boundary */
            size_t align = -reinterpret_cast<uintptr_t>(xs) & (sizeof(size_t) - 1);
            count -= align;
            while (align--)
                *xs++ = b;
    
            /* make word-sized pattern */
            size_t pattern = (b << 24) | (b << 16) | (b << 8) | b;
            pattern = (pattern << 32) | pattern;
    
            auto *xw = reinterpret_cast<size_t *>(xs);
            while (count >= sizeof(size_t))
            {
                *xw++ = pattern;
                count -= sizeof(size_t);
            }
    
            /* fill the remaining bytes */
            xs = reinterpret_cast<uint8_t *>(xw);
            while (count--)
                *xs++ = b;
    
            return s;
        }
    }

    static uint64_t hhdm_offset = 0;
    static uint64_t *kernel_pml4 = nullptr;
    static const VmmFlags KERNEL_FLAGS_NEW = KERNEL_RW;
    static constexpr uint64_t KERNEL_FLAGS = x86_64_internal::VMM_PRESENT | x86_64_internal::VMM_WRITABLE;

    /* memory regions */
    static constexpr auto KERNEL_HEAP_START = 0xFFFF8F0000000000;
    static constexpr auto KERNEL_HEAP_END = 0xFFFF900000000000;

    /* paging structure */
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t PAGE_TABLE_ENTRIES = 512;
    
    static constexpr size_t MAX_REGIONS = 64;
    static MemoryRegion regions[MAX_REGIONS];
    static size_t region_count = 0;

    static uintptr_t find_free_region(size_t size)
    {
        for (size_t i = 0; i < region_count; i++)
        {
            if (!regions[i].used && (regions[i].end - regions[i].start >= size))
            {
                /* found a region large enough */
                uintptr_t addr = regions[i].start;

                /* split the region if it's larger than needed */
                if (regions[i].end - regions[i].start > size)
                {
                    /* create new region for the remainder */
                    if (region_count < MAX_REGIONS)
                    {
                        /* shift other regions */
                        for (size_t j = region_count; j > i + 1; j--)
                        {
                            regions[j] = regions[j - 1];
                        }

                        /* insert new region */
                        regions[i + 1] = {
                            .start = regions[i].start + size,
                            .end = regions[i].end,
                            .used = false
                        };

                        /* update original region */
                        regions[i].end = regions[i].start + size;
                        region_count++;
                    }
                }

                /* mark region as used */
                regions[i].used = true;
                return addr;
            }
        }

        return 0; /* no suitable region found */
    }

    static void free_region(uintptr_t addr, size_t size)
    {
        for (size_t i = 0; i < region_count; i++)
        {
            if (regions[i].start == addr && regions[i].used)
            {
                /* mark as free */
                regions[i].used = false;

                /* try to merge with adjacent free regions */
                /* check next region */
                if (i + 1 < region_count && !regions[i + 1].used && regions[i].end == regions[i + 1].start)
                {
                    regions[i].end = regions[i + 1].end;

                    /* remove merged region */
                    for (size_t j = i + 1; j < region_count - 1; j++)
                        regions[j] = regions[j + 1];
                    region_count--;
                }

                /* check previous region */
                if (i > 0 && !regions[i - 1].used && regions[i - 1].end == regions[i].start)
                {
                    regions[i - 1].end = regions[i].end;

                    /* remove merged region */
                    for (size_t j = i; j < region_count - 1; j++)
                    {
                        regions[j] = regions[j + 1];
                    }
                    region_count--;
                }

                break;
            }
        }
    }

    uint64_t vmm_traits<x86_64>::translate_flags(VmmFlags flags) noexcept
    {
        uint64_t native_flags = 0;
        
        // In x86_64, presence is implied by read
        if (static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::PROT_READ))
            native_flags |= x86_64_internal::VMM_PRESENT;
            
        if (static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::PROT_WRITE))
            native_flags |= x86_64_internal::VMM_WRITABLE;
            
        /* in AMD64, NX flag is inverted (set = no execute) */
        if (!(static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::PROT_EXEC)))
            native_flags |= x86_64_internal::VMM_NX;
            
        /* either KERNEL or USER flag must be set, default to KERNEL */
        if (static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::USER))
            native_flags |= x86_64_internal::VMM_USER;
        
        if (static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::WRITETHROUGH))
            native_flags |= x86_64_internal::VMM_WRITETHROUGH;
            
        if (static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::CACHE_DISABLE))
            native_flags |= x86_64_internal::VMM_CACHE_DISABLE;
            
        if (static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::GLOBAL))
            native_flags |= x86_64_internal::VMM_GLOBAL;
            
        if (static_cast<uint64_t>(flags) & static_cast<uint64_t>(VmmFlags::HUGE))
            native_flags |= x86_64_internal::VMM_HUGE;
            
        return native_flags;
    }

    void vmm_traits<x86_64>::init(uint64_t offset) noexcept
    {
        hhdm_offset = offset;

        /* get the current PML4 from CR3 */
        kernel_pml4 = reinterpret_cast<uint64_t *>(cpu_traits<x86_64>::read_cr3() + hhdm_offset);
        regions[0] = {
            .start = KERNEL_HEAP_START,
            .end = KERNEL_HEAP_END,
            .used = false
        };
        region_count = 1;
    }

    uintptr_t vmm_traits<x86_64>::map_page(size_t n) noexcept
    {
        if (n == 0)
            return 0;

        const uintptr_t phys_addr = pmm::pmalloc(n);
        if (phys_addr == 0)
            return 0; /* failed to allocate */

        const uintptr_t virt_addr = find_free_region(n * PAGE_SIZE);
        if (virt_addr == 0)
        {
            /* release the resource since we couldn't find virtual space */
            pmm::pfree(phys_addr, n);
            return 0;
        }

        for (size_t i = 0; i < n; ++i)
            map_page_internal(virt_addr + i * PAGE_SIZE, phys_addr + i * PAGE_SIZE, KERNEL_FLAGS);

        return virt_addr;
    }

    void vmm_traits<x86_64>::map_page(uintptr_t virt_addr, uintptr_t phys_addr, VmmFlags flags) noexcept
    {
        map_page_internal(virt_addr, phys_addr, translate_flags(flags));
    }

    void vmm_traits<x86_64>::map_page_internal(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t flags) noexcept
    {
        /* calculate indices for each level */
        const size_t pml4_index = (virt_addr >> 39) & 0x1FF;
        const size_t pdpt_index = (virt_addr >> 30) & 0x1FF;
        const size_t pd_index = (virt_addr >> 21) & 0x1FF;
        const size_t pt_index = (virt_addr >> 12) & 0x1FF;

        /* get or create PDPT */
        uint64_t &pml4e = kernel_pml4[pml4_index];
        uint64_t *pdpt;

        if (!(pml4e & x86_64_internal::VMM_PRESENT))
        {
            const uintptr_t pdpt_phys = pmm::pmalloc(1);
            if (!pdpt_phys)
                return; /* out of memory */

            pdpt = reinterpret_cast<uint64_t *>(pdpt_phys + hhdm_offset);
            memset(pdpt, 0, PAGE_SIZE);

            pml4e = pdpt_phys | x86_64_internal::VMM_PRESENT | x86_64_internal::VMM_WRITABLE | (flags & x86_64_internal::VMM_USER);
        }
        else
        {
            pdpt = reinterpret_cast<uint64_t *>((pml4e & ~0xFFF) + hhdm_offset);
        }

        /* get or create PD */
        uint64_t &pdpte = pdpt[pdpt_index];
        uint64_t *pd;

        if (!(pdpte & x86_64_internal::VMM_PRESENT))
        {
            const uintptr_t pd_phys = pmm::pmalloc(1);
            if (!pd_phys)
                return; /* out of memory */

            pd = reinterpret_cast<uint64_t *>(pd_phys + hhdm_offset);
            memset(pd, 0, PAGE_SIZE);

            pdpte = pd_phys | x86_64_internal::VMM_PRESENT | x86_64_internal::VMM_WRITABLE | (flags & x86_64_internal::VMM_USER);
        }
        else
        {
            pd = reinterpret_cast<uint64_t *>((pdpte & ~0xFFF) + hhdm_offset);
        }

        /* get or create PT */
        uint64_t &pde = pd[pd_index];
        uint64_t *pt;

        if (!(pde & x86_64_internal::VMM_PRESENT))
        {
            const uintptr_t pt_phys = pmm::pmalloc(1);
            if (!pt_phys)
                return; /* out of memory */

            pt = reinterpret_cast<uint64_t *>(pt_phys + hhdm_offset);
            memset(pt, 0, PAGE_SIZE);

            pde = pt_phys | x86_64_internal::VMM_PRESENT | x86_64_internal::VMM_WRITABLE | (flags & x86_64_internal::VMM_USER);
        }
        else
        {
            pt = reinterpret_cast<uint64_t *>((pde & ~0xFFF) + hhdm_offset);
        }

        /* set the page table entry */
        pt[pt_index] = phys_addr | flags;

        /* invalidate TLB for this page */
        cpu_traits<x86_64>::invlpg(reinterpret_cast<void *>(virt_addr));
    }

    void vmm_traits<x86_64>::unmap_page(uintptr_t virt_addr) noexcept
    {
        /* find the region containing this address */
        for (size_t i = 0; i < region_count; i++)
        {
            if (regions[i].start <= virt_addr && virt_addr < regions[i].end && regions[i].used)
            {
                const size_t pages = (regions[i].end - regions[i].start) / PAGE_SIZE; /* found the region */

                const size_t BATCH_SIZE = 64;
                for (size_t batch_start = 0; batch_start < pages; batch_start += BATCH_SIZE)
                {
                    const size_t batch_end = (batch_start + BATCH_SIZE < pages) ? batch_start + BATCH_SIZE : pages;
                    uintptr_t phys_addrs[BATCH_SIZE]; /* stack allocation with reasonable size */

                    /* get physical addresses for this batch */
                    for (size_t j = batch_start; j < batch_end; j++)
                        phys_addrs[j - batch_start] = get_pmaddr(virt_addr + j * PAGE_SIZE);

                    /* unmap all pages in this batch */
                    for (size_t j = batch_start; j < batch_end; j++)
                    {
                        /* calculate indices */
                        const uintptr_t curr_addr = virt_addr + j * PAGE_SIZE;
                        const size_t pml4_index = (curr_addr >> 39) & 0x1FF;
                        const size_t pdpt_index = (curr_addr >> 30) & 0x1FF;
                        const size_t pd_index = (curr_addr >> 21) & 0x1FF;
                        const size_t pt_index = (curr_addr >> 12) & 0x1FF;

                        /* check if PML4 entry exists */
                        if (!(kernel_pml4[pml4_index] & x86_64_internal::VMM_PRESENT))
                            continue; /* already unmapped */

                        /* navigate to page table */
                        const auto *pdpt = reinterpret_cast<uint64_t *>(
                            (kernel_pml4[pml4_index] & ~0xFFF) + hhdm_offset);
                        if (!(pdpt[pdpt_index] & x86_64_internal::VMM_PRESENT))
                            continue; /* already unmapped */

                        const auto *pd = reinterpret_cast<uint64_t *>((pdpt[pdpt_index] & ~0xFFF) + hhdm_offset);
                        if (!(pd[pd_index] & x86_64_internal::VMM_PRESENT))
                            continue; /* already unmapped */

                        auto *pt = reinterpret_cast<uint64_t *>((pd[pd_index] & ~0xFFF) + hhdm_offset);

                        /* clear the page table entry */
                        pt[pt_index] = 0;

                        /* invalidate TLB entry */
                        cpu_traits<x86_64>::invlpg(reinterpret_cast<void *>(curr_addr));
                    }

                    /* free the physical pages for this batch */
                    for (size_t j = batch_start; j < batch_end; j++)
                    {
                        if (phys_addrs[j - batch_start])
                            pmm::pfree(phys_addrs[j - batch_start], 1);
                    }
                }

                /* free the virtual memory region */
                free_region(virt_addr, pages * PAGE_SIZE);
                break;
            }
        }
    }

    uintptr_t vmm_traits<x86_64>::get_pmaddr(uintptr_t virt_addr) noexcept
    {
        /* calculate indices */
        const size_t pml4_index = (virt_addr >> 39) & 0x1FF;
        const size_t pdpt_index = (virt_addr >> 30) & 0x1FF;
        const size_t pd_index = (virt_addr >> 21) & 0x1FF;
        const size_t pt_index = (virt_addr >> 12) & 0x1FF;

        /* check if PML4 entry exists */
        if (!(kernel_pml4[pml4_index] & x86_64_internal::VMM_PRESENT))
        {
            return 0; /* not mapped */
        }

        /* navigate through paging structures */
        const auto *pdpt = reinterpret_cast<uint64_t *>((kernel_pml4[pml4_index] & ~0xFFF) + hhdm_offset);
        if (!(pdpt[pdpt_index] & x86_64_internal::VMM_PRESENT))
            return 0; /* not mapped */

        /* check for 1GB page */
        if (pdpt[pdpt_index] & x86_64_internal::VMM_HUGE)
            return (pdpt[pdpt_index] & ~0x3FFFFFFF) + (virt_addr & 0x3FFFFFFF);

        const auto *pd = reinterpret_cast<uint64_t *>((pdpt[pdpt_index] & ~0xFFF) + hhdm_offset);
        if (!(pd[pd_index] & x86_64_internal::VMM_PRESENT))
            return 0; /* not mapped */

        /* check for 2MB page */
        if (pd[pd_index] & x86_64_internal::VMM_HUGE)
            return (pd[pd_index] & ~0x1FFFFF) + (virt_addr & 0x1FFFFF);

        /* regular 4KB page */
        const auto *pt = reinterpret_cast<uint64_t *>((pd[pd_index] & ~0xFFF) + hhdm_offset);
        if (!(pt[pt_index] & x86_64_internal::VMM_PRESENT))
            return 0; /* not mapped */

        return (pt[pt_index] & ~0xFFF) + (virt_addr & 0xFFF);
    }

    uintptr_t vmm_traits<x86_64>::create_ptb() noexcept
    {
        /* allocate physical memory for new PML4 */
        const uintptr_t pml4_phys = pmm::pmalloc(1);
        if (!pml4_phys)
            return 0;

        auto *new_pml4 = reinterpret_cast<uint64_t *>(pml4_phys + hhdm_offset);

        /* clear the table */
        memset(new_pml4, 0, PAGE_SIZE);

        /* copy kernel entries (typically higher half) */
        /* for x86_64, kernel space usually starts at entry 256 */
        for (size_t i = 256; i < PAGE_TABLE_ENTRIES; i++)
            new_pml4[i] = kernel_pml4[i];

        return pml4_phys;
    }

    void vmm_traits<x86_64>::switch_ptb(uintptr_t ptb_phys) noexcept
    {
        cpu_traits<x86_64>::write_cr3(ptb_phys);
    }
}
