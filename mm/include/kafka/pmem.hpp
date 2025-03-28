/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <stdint.h>
#include <limine.h>

namespace kfk
{
    class PhysicalPageManager
    {
    public:
        static bool init(volatile limine_memmap_request* mmap, uint64_t offset) noexcept;

        static uintptr_t pmalloc(uint64_t n = 1) noexcept;

        static void pfree(uintptr_t base, uint64_t n = 1) noexcept;

        static void* phys_to_virt(uintptr_t phys) noexcept;

        static void dynamic_mode() noexcept;
    };
    
    using pmm = PhysicalPageManager;
}