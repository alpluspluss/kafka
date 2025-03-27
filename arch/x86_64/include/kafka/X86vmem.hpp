/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kafka/types.hpp>
#include <kafka/hal/vmem.hpp>

namespace kfk
{
    namespace x86_64_internal
    {
        /* x86_64 native paging flags */
        static constexpr uint64_t VMM_PRESENT = 1ULL << 0;
        static constexpr uint64_t VMM_WRITABLE = 1ULL << 1;
        static constexpr uint64_t VMM_USER = 1ULL << 2;
        static constexpr uint64_t VMM_WRITETHROUGH = 1ULL << 3;
        static constexpr uint64_t VMM_CACHE_DISABLE = 1ULL << 4;
        static constexpr uint64_t VMM_ACCESSED = 1ULL << 5;
        static constexpr uint64_t VMM_DIRTY = 1ULL << 6;
        static constexpr uint64_t VMM_HUGE = 1ULL << 7;
        static constexpr uint64_t VMM_GLOBAL = 1ULL << 8;
        static constexpr uint64_t VMM_NX = 1ULL << 63;
    }

    template<>
    class vmm_traits<x86_64>
    {
    public:
        static uint64_t translate_flags(VmmFlags flags) noexcept;
        
        static void init(uint64_t offset) noexcept;

        static uintptr_t map_page(size_t n = 1) noexcept;
        
        static void map_page(uintptr_t virt_addr, uintptr_t phys_addr, VmmFlags flags) noexcept;
        
        static void map_page_internal(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t native_flags) noexcept;

        static void unmap_page(uintptr_t virt_addr) noexcept;

        static uintptr_t get_pmaddr(uintptr_t virt_addr) noexcept;

        static uintptr_t create_ptb() noexcept;

        static void switch_ptb(uintptr_t ptb_phys) noexcept;
    };
}
