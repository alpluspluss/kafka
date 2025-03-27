/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
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
        static void init(uint64_t offset) noexcept;

        /* CPU utils */
        [[noreturn]] static void halt() noexcept;

        static void pause() noexcept;
    };

    using cpu = cpu_traits<current_arch>;
}
