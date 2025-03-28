/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <iostream.hpp>
#include <string.hpp>
#include <kafka/pmem.hpp>
#include <kafka/region.hpp>
#include <kafka/slub.hpp>

namespace kfk
{
    static constexpr size_t PAGE_SIZE = 4096;
    static uint64_t hhdm_offset = 0;

    bool PhysicalPageManager::init(volatile limine_memmap_request *mmap, uint64_t offset) noexcept
    {
        const limine_memmap_response* response = mmap->response;
        if (!response)
            return false;

        hhdm_offset = offset;
        if (!RegionManager::init(64))
            return false;

        for (size_t i = 0; i < response->entry_count; i++)
        {
            const auto entry = response->entries[i];
            if (entry->type == LIMINE_MEMMAP_USABLE)
            {
                const uintptr_t base = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                const uintptr_t end = (entry->base + entry->length) & ~(PAGE_SIZE - 1);
                
                if (end > base)
                    /* add to region manager */
                    RegionManager::add(base, end - base, true);
            }
        }

        RegionManager::sort();
        RegionManager::merge_adjacent();
        //RegionManager::dump();
        return true;
    }

    uintptr_t PhysicalPageManager::pmalloc(const uint64_t n) noexcept
    {
        if (n == 0)
            return 0;
            
        const size_t size = n * PAGE_SIZE;
        Region* region = RegionManager::find_best_fit(size);
        if (!region)
            return 0;
            
        const uintptr_t alloc_base = region->base;
        
        if (region->len == size)
        {
            region->set_free(false);
        }
        else
        {
            if (!RegionManager::split(region, size))
                return 0;
                
            region->set_free(false);
        }
        
        constexpr size_t CACHE_LINE = 64;
        auto* ptr = reinterpret_cast<volatile uint8_t*>(alloc_base + hhdm_offset);
        
        for (size_t offset = 0; offset < size; offset += CACHE_LINE)
        {
            for (size_t i = 0; i < CACHE_LINE && offset + i < size; i++)
                ptr[offset + i] = 0;
        }
        
        return alloc_base;
    }

    void PhysicalPageManager::pfree(uintptr_t base, const uint64_t n) noexcept
    {
        if (base == 0 || n == 0)
            return;
            
        const size_t size = n * PAGE_SIZE;
        Region* region = RegionManager::find(base);
        if (!region || region->is_free())
            return;
            
        if (region->len == size)
        {
            region->set_free(true);
        }
        else if (size < region->len)
        {
            if (!RegionManager::split(region, size))
                return;
                
            region->set_free(true);
        }
        else
        {
            return;
        }
        
        RegionManager::merge_adjacent();
    }

    void* PhysicalPageManager::phys_to_virt(uintptr_t phys) noexcept
    {
        return reinterpret_cast<void*>(phys + hhdm_offset);
    }
    
    void PhysicalPageManager::dynamic_mode() noexcept
    {
        Slub::init(); /* note: no side effect if it's already initialized*/
        RegionManager::use_dynamic(); /* switch to dynamic mode */
    }
}
