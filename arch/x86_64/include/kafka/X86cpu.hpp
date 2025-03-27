/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <kafka/types.hpp>
#include <kafka/hal/cpu.hpp>

namespace kfk
{
    template<>
    class cpu_traits<x86_64>
    {
	public:
        static void init(uint64_t offset) noexcept;

		/* CPU utils */
		[[noreturn]] static void halt() noexcept;

        static uint64_t rdmsr(uint32_t msr) noexcept;

		static void wrmsr(uint32_t msr, uint64_t value) noexcept;

		static void invlpg(void* addr) noexcept;

		static void ltr(uint16_t selector) noexcept;

		static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx,
								  uint32_t *ecx,
								  uint32_t *edx) noexcept;

		static uint64_t read_cr0() noexcept;

		static uint64_t read_cr2() noexcept;

		static uint64_t read_cr3() noexcept;

		static uint64_t read_cr4() noexcept;

		static void write_cr0(uint64_t value) noexcept;

		static void write_cr3(uint64_t value) noexcept;

		static void write_cr4(uint64_t value) noexcept;

		static uint64_t xgetbv(uint32_t xcr) noexcept;

		static void xsetbv(uint32_t xcr, uint64_t value) noexcept;

		static void pause() noexcept;
    };
}