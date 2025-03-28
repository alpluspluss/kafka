/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <iostream.hpp>
#include <string.hpp>
#include <kafka/region.hpp>

namespace kfk
{
    RegionManager::RegionAlloc RegionManager::region_alloc;
    Region* RegionManager::regions = nullptr;
    size_t RegionManager::capacity = 0;
    size_t RegionManager::count = 0;

    bool RegionManager::init(size_t initial_capacity) noexcept
    {
        if (regions != nullptr)
            return true; /* already intiialized */
        
        regions = static_cast<Region*>(region_alloc.allocate(initial_capacity * sizeof(Region)));
        if (!regions)
            return false;
            
        capacity = initial_capacity;
        count = 0;
        
        /* init all regions to zero */
        memset(regions, 0, capacity * sizeof(Region));
        return true;
    }

    void RegionManager::use_dynamic() noexcept
    {
        region_alloc.use_dynamic();
    }

    bool RegionManager::add(uintptr_t base, size_t len, bool is_free) noexcept
    {
        if (count >= capacity) /* if capacity not enough */
        {
            if (!grow(capacity * 2))
                return false;
        }
        
        /* add the new region */
        regions[count].base = base;
        regions[count].len = len;
        regions[count].flags = is_free ? Region::FLAG_FREE : 0;
        count++;
        
        return true;
    }

    Region* RegionManager::find(uintptr_t base) noexcept
    {
        /* binary search since regions should be sorted by base address */
        size_t left = 0;
        size_t right = count > 0 ? count - 1 : 0;
        
        while (left <= right && right < count)
        {
            const size_t mid = left + (right - left) / 2;
            const uintptr_t mid_base = regions[mid].base;
            
            if (mid_base == base)
                return &regions[mid];
                
            if (mid_base < base)
                left = mid + 1;
            else
                right = mid > 0 ? mid - 1 : 0;
        }
        
        return nullptr; /* not found */
    }

    Region* RegionManager::find_best_fit(size_t size) noexcept
    {
        size_t best_index = count;
        size_t smallest_fit = SIZE_MAX;
        
        for (size_t i = 0; i < count; i++)
        {
            if (regions[i].is_free() && regions[i].len >= size)
            {
                if (regions[i].len < smallest_fit)
                {
                    smallest_fit = regions[i].len;
                    best_index = i;
                }
            }
        }
        
        return (best_index < count) ? &regions[best_index] : nullptr;
    }

    void RegionManager::sort() noexcept
    {
        /* simple insertion sort - the list is usually small */
        for (size_t i = 1; i < count; i++)
        {
            Region key = regions[i];
            size_t j = i;
            
            while (j > 0 && regions[j - 1].base > key.base)
            {
                regions[j] = regions[j - 1];
                j--;
            }
            
            regions[j] = key;
        }
    }

    void RegionManager::merge_adjacent() noexcept
    {
        if (count <= 1)
            return;
            
        sort(); /* always need to be sorted for optimal perf */
        for (size_t i = 0; i < count - 1; ) /* merge the FREEs */
        {
            Region& curr = regions[i];
            Region& next = regions[i + 1];
            
            if (curr.base + curr.len == next.base && 
                curr.is_free() == next.is_free())
            {
                /* merge by extending current region */
                curr.len += next.len;
                
                /* remove the next region by shifting all following regions */
                for (size_t j = i + 1; j < count - 1; j++)
                    regions[j] = regions[j + 1];
                    
                count--;
                /* don't increment i, check if the merged region can merge with next */
            }
            else
            {
                /* only move to next region if no merge happened */
                i++;
            }
        }
    }

    bool RegionManager::split(Region* region, size_t offset) noexcept
    {
        if (!region || offset >= region->len)
            return false;
            
        if (count >= capacity)
        {
            size_t region_index = region - regions;
            
            if (!grow(capacity * 2))
                return false;
                
            region = &regions[region_index];
        }
        
        uintptr_t second_base = region->base + offset;
        size_t second_len = region->len - offset;
        bool is_free = region->is_free();
        
        region->len = offset;

        size_t region_index = region - regions;
        for (size_t i = count; i > region_index + 1; i--)
            regions[i] = regions[i - 1];
            
        regions[region_index + 1].base = second_base;
        regions[region_index + 1].len = second_len;
        regions[region_index + 1].flags = is_free ? Region::FLAG_FREE : 0;
        
        count++;
        return true;
    }

    bool RegionManager::grow(size_t new_capacity) noexcept
    {
        Region* new_regions = static_cast<Region*>(
            region_alloc.allocate(new_capacity * sizeof(Region)));
            
        if (!new_regions)
            return false;
            
        memcpy(new_regions, regions, count * sizeof(Region));
        memset(&new_regions[count], 0, (new_capacity - count) * sizeof(Region));
        
        region_alloc.free(regions);
        
        regions = new_regions;
        capacity = new_capacity;
        return true;
    }

    void RegionManager::dump() noexcept
    {
        kfk::println("memory regions:");
        for (size_t i = 0; i < count; i++)
        {
            const Region& r = regions[i];
            kfk::printf("  region %d: base=%x len=%x %s\n", 
                      i, r.base, r.len, r.is_free() ? "free" : "used");
        }
    }
}
