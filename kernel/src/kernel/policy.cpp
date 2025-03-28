/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <allocator.hpp>
#include <kernel/policy.hpp>

namespace kfk
{
    void enable_dynamic_allocation() noexcept
    {
        kernel_allocator.use_dynamic();
    }
}
