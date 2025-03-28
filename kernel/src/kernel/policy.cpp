/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <allocator.hpp>
#include <kafka/slub.hpp>
#include <kafka/pmem.hpp>
#include <kernel/policy.hpp>
#include <kafka/hal/vmem.hpp>

namespace policy
{
    void dynamic_alloc() noexcept
    {
        kfk::pmm::dynamic_mode();
        kfk::vmm::dynamic_mode();
        kfk::Slub::use_dynamic();
        kfk::kernel_allocator.use_dynamic(); /* this is for shared kernel objects */
    }
}
