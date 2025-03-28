/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <allocator.hpp>

namespace kfk
{
    struct Region
    {
        uintptr_t base;
        size_t len;
        uint8_t flags;
        
        static constexpr uint8_t FLAG_FREE = 0x1;
        
        /* helper methods */
        [[nodiscard]] bool is_free() const 
        { 
            return flags & FLAG_FREE; 
        }
        
        void set_free(bool free) 
        { 
            if (free)
                flags |= FLAG_FREE;
            else
                flags &= ~FLAG_FREE;
        }
    };

    class RegionManager
    {
    public:
        static bool init(size_t initial_capacity = 64) noexcept;
        
        static void use_dynamic() noexcept;
        
        static bool add(uintptr_t base, size_t len, bool is_free = true) noexcept;
        
        static Region* find(uintptr_t base) noexcept;
        
        static Region* find_best_fit(size_t size) noexcept;
        
        static void sort() noexcept;
        
        static void merge_adjacent() noexcept;
        
        static bool split(Region* region, size_t offset) noexcept;
        
        static void dump() noexcept;

    private:

        using RegionAlloc = Allocator<AllocPolicy::SWITCHABLE, 8 * 1024>;
        static RegionAlloc region_alloc;

        static Region* regions;
        static size_t capacity;
        static size_t count;

        static bool grow(size_t new_capacity) noexcept;
    };
}
