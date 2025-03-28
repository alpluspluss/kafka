/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
#include <kafka/types.hpp>

namespace kfk
{
    template<typename Arch>
    class io_traits
    {
    public:
        static uint8_t inb(uint16_t port) noexcept;
        static void outb(uint16_t port, uint8_t value) noexcept;
        
        static uint16_t inw(uint16_t port) noexcept;
        static void outw(uint16_t port, uint16_t value) noexcept;
        
        static uint32_t inl(uint16_t port) noexcept;
        static void outl(uint16_t port, uint32_t value) noexcept;
        
        static void wait() noexcept;
    };
    
    using io = io_traits<current_arch>;
}
