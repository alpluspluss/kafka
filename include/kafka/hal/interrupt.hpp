/* this file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

/* note: it is recommended to check `$(ROOT)/docs` first before doing anything */

#pragma once

#include <kafka/types.hpp>
#include <limine.h>
#include <stdint.h>

namespace kfk
{
	enum Vint : uint16_t
	{
		/* synchronous exceptions (0x0000 - 0x00FF) */
		EXCEPTION_DIVIDE_ERROR = 0x0000,
		EXCEPTION_DEBUG = 0x0001,
		EXCEPTION_NMI = 0x0002,
		EXCEPTION_BREAKPOINT = 0x0003,
		EXCEPTION_OVERFLOW = 0x0004,
		EXCEPTION_BOUND_RANGE = 0x0005,
		EXCEPTION_INVALID_OPCODE = 0x0006,
		EXCEPTION_DEVICE_NA = 0x0007,
		EXCEPTION_DOUBLE_FAULT = 0x0008,
		EXCEPTION_COPROC_SEG = 0x0009,
		EXCEPTION_INVALID_TSS = 0x000A,
		EXCEPTION_SEG_NOT_PRESENT = 0x000B,
		EXCEPTION_STACK_FAULT = 0x000C,
		EXCEPTION_GENERAL_PROTECTION = 0x000D,
		EXCEPTION_PAGE_FAULT = 0x000E,
		EXCEPTION_FPU_ERROR = 0x000F,
		EXCEPTION_ALIGNMENT_CHECK = 0x0010,
		EXCEPTION_MACHINE_CHECK = 0x0011,
		EXCEPTION_SIMD_FP = 0x0012,
		EXCEPTION_VIRT = 0x0013,
		EXCEPTION_SECURITY = 0x0014,

		/* hardware interrupts (0x0100 - 0x01FF) */
		IRQ_TIMER = 0x0100,
		IRQ_KEYBOARD = 0x0101,
		IRQ_CASCADE = 0x0102,
		IRQ_COM2 = 0x0103,
		IRQ_COM1 = 0x0104,
		IRQ_LPT2 = 0x0105,
		IRQ_FLOPPY = 0x0106,
		IRQ_LPT1 = 0x0107,
		IRQ_RTC = 0x0108,
		IRQ_PERIPH1 = 0x0109,
		IRQ_PERIPH2 = 0x010A,
		IRQ_PERIPH3 = 0x010B,
		IRQ_MOUSE = 0x010C,
		IRQ_FPU = 0x010D,
		IRQ_PRIMARY_ATA = 0x010E,
		IRQ_SECONDARY_ATA = 0x010F,

		/* software interrupts (0x0200 - 0x02FF) */
		SYSCALL = 0x0200,

		/* platform specific (0x1000 - 0x1FFF) */
		PLAT_SPECIFIC_BASE = 0x1000,

		/* user-defined (0x8000 - 0xFFFF) */
		USER_DEFINED_BASE = 0x8000
	};

    enum class IntFlags : uint32_t
    {
        NONE = 0,
        EDGE_TRIGGER   = 1 << 0,
        LEVEL_TRIGGER  = 1 << 1,
        MASKED         = 1 << 2,
        UNMASKED       = 1 << 3
    };

    using int_handler = void(*)(void*);

    struct InterruptEntry
    {
        int_handler handler; /* handle function*/
        void* context; /* context pased to the handler */
        IntFlags flags; /* behavior flags */
        uint8_t priority; /* priority (0-255)*/
        Vint id; /* virtual interrupt number */
    };

	/* please do not call this */
	template<typename Arch>
	class interrupt_traits
	{
	public:
		static void init() noexcept;

		static void enable(uint16_t n) noexcept; /* enable an interrupt  */

		static void disable(uint16_t n) noexcept; /* disable an interrupt  */

		static void enable() noexcept;

		static void disable() noexcept;

        static void register_handler(Vint id, int_handler handler, 
            void* context = nullptr, 
            uint8_t priority = 128,
            IntFlags flags = IntFlags::NONE) noexcept;
	};

	using interrupt = interrupt_traits<current_arch>;
}
