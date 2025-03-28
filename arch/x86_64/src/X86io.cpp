/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <kafka/X86io.hpp>
#include <kafka/types.hpp>
#include "kafka/hal/io.hpp"

namespace kfk
{
    uint8_t io_traits<x86_64>::inb(uint16_t port) noexcept
    {
        uint8_t ret;
        asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
    }

    void io_traits<x86_64>::outb(uint16_t port, uint8_t value) noexcept
    {
        asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
    }

    uint16_t io_traits<x86_64>::inw(uint16_t port) noexcept
    {
        uint16_t ret;
        asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
    }

    void io_traits<x86_64>::outw(uint16_t port, uint16_t value) noexcept
    {
        asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
    }

    uint32_t io_traits<x86_64>::inl(uint16_t port) noexcept
    {
        uint32_t ret;
        asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
    }

    void io_traits<x86_64>::outl(uint16_t port, uint32_t value) noexcept
    {
        asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
    }

    void io_traits<x86_64>::wait() noexcept
    {
        outb(0x80, 0);
    }
}
