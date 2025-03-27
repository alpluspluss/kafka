/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <kafka/hal/interrupt.hpp>
#include <kafka/types.hpp>

namespace kfk
{
	template<>
	class interrupt_traits<x86_64>
	{
	public:
		static void init() noexcept;

		static void enable(uint16_t n) noexcept;

		static void disable(uint16_t n) noexcept;

		static void enable() noexcept;

		static void disable() noexcept;

		static void register_handler(Vint id, int_handler handler, void *context = nullptr,
									 uint8_t priority = 128, IntFlags flags = IntFlags::NONE) noexcept;

		static uint8_t to_vector(Vint id) noexcept;

		static Vint from_vector(uint8_t vector) noexcept;
	};
}
