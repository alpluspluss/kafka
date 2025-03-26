/* this file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <limine.h>
#include <atomic.hpp>
#include <kafka/hal/cpu.hpp>

namespace
{
    __attribute__((used, section(".limine_requests")))
    volatile LIMINE_BASE_REVISION(3);
    
    __attribute__((used, section(".limine_requests")))
    volatile limine_hhdm_request hhdm_request = {
        .id = LIMINE_HHDM_REQUEST,
        .revision = 0,
        .response = nullptr
    };
}

extern "C" void kernel_main()
{
    if (LIMINE_BASE_REVISION_SUPPORTED == false)
        kfk::cpu::halt();

    kfk::cpu::init(&hhdm_request);
    kfk::cpu::halt();
}