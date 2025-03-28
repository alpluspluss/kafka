/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kafka/types.hpp>

namespace kfk
{
    struct MemoryRegion
    {
        uintptr_t start;
        uintptr_t end;
        bool used;
    };

    /* arch-independent VMM flags for POSIX compatibility */
    enum class VmmFlags : uint64_t
    {
        NONE = 0,
        
        /* POSIX protection flags */
        PROT_READ = 1ULL << 0,        
        PROT_WRITE = 1ULL << 1,       
        PROT_EXEC = 1ULL << 2,
        
        /* POSIX mmap flags */
        MAP_SHARED = 1ULL << 8, 
        MAP_PRIVATE = 1ULL << 9, 
        MAP_FIXED = 1ULL << 10, 
        MAP_ANONYMOUS = 1ULL << 11,
        
        /* Extended flags (for kernel use) */
        CACHE_DISABLE = 1ULL << 16, 
        WRITETHROUGH = 1ULL << 17, 
        GLOBAL = 1ULL << 18,
        HUGE = 1ULL << 19, 
        KERNEL = 1ULL << 20, 
        USER = 1ULL << 21 
    };

    inline VmmFlags operator|(VmmFlags a, VmmFlags b) 
    {
        return static_cast<VmmFlags>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    inline VmmFlags operator&(VmmFlags a, VmmFlags b) 
    {
        return static_cast<VmmFlags>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
    }

    inline VmmFlags& operator|=(VmmFlags& a, VmmFlags b) 
    {
        a = a | b;
        return a;
    }

    inline bool operator&(VmmFlags a, uint64_t b) 
    {
        return (static_cast<uint64_t>(a) & b) != 0;
    }

    /* standard protection combinations */
    static constexpr VmmFlags PROT_NONE = VmmFlags::NONE;
    static const VmmFlags PROT_READ_WRITE = VmmFlags::PROT_READ | VmmFlags::PROT_WRITE;
    static const VmmFlags PROT_READ_EXEC = VmmFlags::PROT_READ | VmmFlags::PROT_EXEC;
    static const VmmFlags PROT_READ_WRITE_EXEC = VmmFlags::PROT_READ | VmmFlags::PROT_WRITE | VmmFlags::PROT_EXEC;

    /* common kernel mapping flags */
    static const VmmFlags KERNEL_RW = VmmFlags::PROT_READ | VmmFlags::PROT_WRITE | VmmFlags::KERNEL;
    static const VmmFlags KERNEL_RX = VmmFlags::PROT_READ | VmmFlags::PROT_EXEC | VmmFlags::KERNEL;
    static const VmmFlags USER_RW = VmmFlags::PROT_READ | VmmFlags::PROT_WRITE | VmmFlags::USER;
    static const VmmFlags USER_RX = VmmFlags::PROT_READ | VmmFlags::PROT_EXEC | VmmFlags::USER;

    template<typename Arch>
    class vmm_traits
    {
    public:
        static uint64_t translate_flags(VmmFlags flags) noexcept;
        
        static void init(uint64_t offset) noexcept;
        
        static uintptr_t map_page(size_t n = 1) noexcept;
        
        static void map_page(uintptr_t virt_addr, uintptr_t phys_addr, VmmFlags flags) noexcept;
        
        static void unmap_page(uintptr_t virt_addr) noexcept;
        
        static uintptr_t get_pmaddr(uintptr_t virt_addr) noexcept;

        static void dynamic_mode() noexcept;
        
        /* page table operations for proc mm */
        static uintptr_t create_ptb() noexcept;

        static void switch_ptb(uintptr_t ptb_phys) noexcept;
    };

    using vmm = vmm_traits<current_arch>;
}
