/* this file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <limine.h>
#include <kafka/types.hpp>

namespace kfk
{
    /* please do not call this */
    template<typename Arch>
    class cpu_traits
    {
    public:
        /* initialize CPU, add */
        static void init(volatile limine_hhdm_request* hhdm) noexcept;

        /* CPU utils */
        [[noreturn]] static void halt() noexcept;

        static void pause() noexcept;
    };

    using cpu = cpu_traits<current_arch>;
}
