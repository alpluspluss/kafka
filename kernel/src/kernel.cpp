/* this file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <limine.h>
#include <kafka/hal/cpu.hpp>
#include <kafka/hal/interrupt.hpp>

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

    uint64_t hhdm_offset = hhdm_request.response->offset;
    if (!hhdm_offset)
        goto cooked;

    kfk::cpu::init(hhdm_offset);

    kfk::interrupt::init();
    kfk::cpu::pause();

cooked: /* 
         * all error handling that relates to intialization should be here; 
         * this reduces some code boilerplates
         */
    kfk::cpu::halt();
}
