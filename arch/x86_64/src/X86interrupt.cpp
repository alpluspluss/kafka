/* this file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <kafka/X86interrupt.hpp>
#include <kafka/hal/cpu.hpp>
#include <kafka/types.hpp>
#include <stdint.h>

namespace kfk
{
	constexpr uint16_t IDT_ENTRIES = 256;

	struct InterruptFrame
	{
		uint64_t ip;
		uint64_t cs;
		uint64_t flags;
		uint64_t sp;
		uint64_t ss;
	} __attribute__((packed));

	struct IdtEntry
	{
		uint16_t offset_low;
		uint16_t selector;
		uint8_t ist;
		uint8_t flags;
		uint16_t offset_mid;
		uint32_t offset_high;
		uint32_t reserved;
	} __attribute__((packed));

	struct IdtDescriptor
	{
		uint16_t limit;
		uint64_t base;
	} __attribute__((packed));

	namespace
	{
		static IdtEntry idt_entries[IDT_ENTRIES] = {};

		static IdtDescriptor idt_descriptor = { sizeof(idt_entries) - 1, reinterpret_cast<uint64_t>(idt_entries) };

		/* map from Vint to X86 native IDT vector */
		static constexpr uint8_t vint_to_vector[] = {
			/* EXCEPTION_DIVIDE_ERROR        */ 0,
			/* EXCEPTION_DEBUG               */ 1,
			/* EXCEPTION_NMI                 */ 2,
			/* EXCEPTION_BREAKPOINT          */ 3,
			/* EXCEPTION_OVERFLOW            */ 4,
			/* EXCEPTION_BOUND_RANGE         */ 5,
			/* EXCEPTION_INVALID_OPCODE      */ 6,
			/* EXCEPTION_DEVICE_NA           */ 7,
			/* EXCEPTION_DOUBLE_FAULT        */ 8,
			/* EXCEPTION_COPROC_SEG          */ 9,
			/* EXCEPTION_INVALID_TSS         */ 10,
			/* EXCEPTION_SEG_NOT_PRESENT     */ 11,
			/* EXCEPTION_STACK_FAULT         */ 12,
			/* EXCEPTION_GENERAL_PROTECTION  */ 13,
			/* EXCEPTION_PAGE_FAULT          */ 14,
			/* reserved                      */ 15,
			/* EXCEPTION_FPU_ERROR           */ 16,
			/* EXCEPTION_ALIGNMENT_CHECK     */ 17,
			/* EXCEPTION_MACHINE_CHECK       */ 18,
			/* EXCEPTION_SIMD_FP             */ 19,
			/* EXCEPTION_VIRT                */ 20,
			/* EXCEPTION_SECURITY            */ 30,

			/* IRQs map to vectors 32-47 */
			/* IRQ_TIMER                     */ 32,
			/* IRQ_KEYBOARD                  */ 33,
			/* IRQ_CASCADE                   */ 34,
			/* IRQ_COM2                      */ 35,
			/* IRQ_COM1                      */ 36,
			/* IRQ_LPT2                      */ 37,
			/* IRQ_FLOPPY                    */ 38,
			/* IRQ_LPT1                      */ 39,
			/* IRQ_RTC                       */ 40,
			/* IRQ_PERIPH1                   */ 41,
			/* IRQ_PERIPH2                   */ 42,
			/* IRQ_PERIPH3                   */ 43,
			/* IRQ_MOUSE                     */ 44,
			/* IRQ_FPU                       */ 45,
			/* IRQ_PRIMARY_ATA               */ 46,
			/* IRQ_SECONDARY_ATA             */ 47,

			/* SYSCALL uses int 0x80 (128) on x86 Linux-compatibility */
			/* SYSCALL                       */ 128
		};

		/* reverse mapping: vector back to Vint */
		static constexpr Vint vector_to_vint[256] = {
			/* 0 */ EXCEPTION_DIVIDE_ERROR,
			/* 1 */ EXCEPTION_DEBUG,
			/* 2 */ EXCEPTION_NMI,
			/* 3 */ EXCEPTION_BREAKPOINT,
			/* 4 */ EXCEPTION_OVERFLOW,
			/* 5 */ EXCEPTION_BOUND_RANGE,
			/* 6 */ EXCEPTION_INVALID_OPCODE,
			/* 7 */ EXCEPTION_DEVICE_NA,
			/* 8 */ EXCEPTION_DOUBLE_FAULT,
			/* 9 */ EXCEPTION_COPROC_SEG,
			/* 10 */ EXCEPTION_INVALID_TSS,
			/* 11 */ EXCEPTION_SEG_NOT_PRESENT,
			/* 12 */ EXCEPTION_STACK_FAULT,
			/* 13 */ EXCEPTION_GENERAL_PROTECTION,
			/* 14 */ EXCEPTION_PAGE_FAULT,
			/* 15 */ static_cast<Vint>(0), // Reserved
			/* 16 */ EXCEPTION_FPU_ERROR,
			/* 17 */ EXCEPTION_ALIGNMENT_CHECK,
			/* 18 */ EXCEPTION_MACHINE_CHECK,
			/* 19 */ EXCEPTION_SIMD_FP,
			/* 20 */ EXCEPTION_VIRT,
			/* *******21-29********* */
			static_cast<Vint>(0), static_cast<Vint>(0), static_cast<Vint>(0), static_cast<Vint>(0),
			static_cast<Vint>(0), static_cast<Vint>(0), static_cast<Vint>(0), static_cast<Vint>(0),
			static_cast<Vint>(0),
			/* ********************** */
			/* 30 */ EXCEPTION_SECURITY,
			/* 31 */ static_cast<Vint>(0), /* reserved */
			/* 32 */ IRQ_TIMER,
			/* 33 */ IRQ_KEYBOARD,
			/* 34 */ IRQ_CASCADE,
			/* 35 */ IRQ_COM2,
			/* 36 */ IRQ_COM1,
			/* 37 */ IRQ_LPT2,
			/* 38 */ IRQ_FLOPPY,
			/* 39 */ IRQ_LPT1,
			/* 40 */ IRQ_RTC,
			/* 41 */ IRQ_PERIPH1,
			/* 42 */ IRQ_PERIPH2,
			/* 43 */ IRQ_PERIPH3,
			/* 44 */ IRQ_MOUSE,
			/* 45 */ IRQ_FPU,
			/* 46 */ IRQ_PRIMARY_ATA,
			/* 47 */ IRQ_SECONDARY_ATA,
			/* 48-127: NULL[] */
			/* 128 */ SYSCALL,
			/* 129-255: NULL[] */
		};

		/* handlers for interrupts */
		static int_handler handlers[IDT_ENTRIES] = {};
		static void *handler_contexts[IDT_ENTRIES] = {};
		static IntFlags handler_flags[IDT_ENTRIES] = {};
		static uint8_t handler_priorities[IDT_ENTRIES] = {};

		/* default handlers */
		static void default_handler(void *context)
		{
			/* get interrupt vector from context (which is the frame) */
			InterruptFrame *frame = static_cast<InterruptFrame *>(context);
			/* log unhandled interrupt */
			/* kfk::kprintf("unhandled interrupt at rip: 0x%lx\n", frame->ip); */
		}

		/* trampoline functions to get interrupt number */
		__attribute__((interrupt)) static void int_trampoline(InterruptFrame *frame)
		{
			/* get interrupt number pushed by our handler */
			uint64_t int_no;
			asm volatile("mov 8(%%rbp), %0" : "=r"(int_no) : : "memory");

			if (int_no < IDT_ENTRIES && handlers[int_no])
			{
				handlers[int_no](frame);
			}
			else
			{
				default_handler(frame);
			}
		}

		__attribute__((interrupt)) static void error_trampoline(InterruptFrame *frame, uint64_t error)
		{
			/* get interrupt number pushed by our handler */
			uint64_t int_no;
			asm volatile("mov 16(%%rbp), %0" : "=r"(int_no) : : "memory");

			/* create context with frame and error */
			struct ErrorContext
			{
				InterruptFrame *frame;
				uint64_t error;
			} ctx = { frame, error };

			if (int_no < IDT_ENTRIES && handlers[int_no])
			{
				handlers[int_no](&ctx);
			}
			else
			{
				default_handler(frame);
			}
		}

		/* exception handlers */
		__attribute__((interrupt)) static void page_fault_handler(InterruptFrame *frame, uint64_t error)
		{
			uint64_t fault_addr;
			asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

			/* kfk::kprintf("page fault at 0x%lx (", fault_addr);
			if (error & (1 << 0))
				kfk::kprintf("protection violation, ");
			else
				kfk::kprintf("non-present page, ");

			if (error & (1 << 1))
				kfk::kprintf("write, ");
			else
				kfk::kprintf("read, ");

			if (error & (1 << 2))
			    kfk::kprintf("user, ");
			else
				kfk::kprintf("kernel, ");

			kfk::kprintf(")\n"); */

			cpu_traits<x86_64>::halt();
		}

		__attribute__((interrupt)) static void general_protection_handler(InterruptFrame *frame, uint64_t error)
		{
			/* kfk::kprintf("general protection fault! error code: 0x%lx at rip: 0x%lx\n",
				   error, frame->ip); */
			cpu_traits<x86_64>::halt();
		}

		__attribute__((interrupt)) static void double_fault_handler(InterruptFrame *frame, uint64_t)
		{
			/* kfk::printf("double fault at rip: 0x%lx\n", frame->ip); */
			cpu_traits<x86_64>::halt();
		}

		/* set an idt entry */
		static void set_idt_entry(uint8_t vector, void *handler, uint8_t ist = 0, uint8_t dpl = 0)
		{
			auto &entry = idt_entries[vector];
			uintptr_t handler_addr = reinterpret_cast<uintptr_t>(handler);

			entry.offset_low = handler_addr & 0xFFFF;
			entry.selector = 0x08; /* kernel code segment */
			entry.ist = ist & 0x7;
			entry.flags = 0x8E | ((dpl & 0x3) << 5); /* PRESENT | INT_GATE | DPI */
			entry.offset_mid = (handler_addr >> 16) & 0xFFFF;
			entry.offset_high = (handler_addr >> 32) & 0xFFFFFFFF;
			entry.reserved = 0;
		}
	}

	void interrupt_traits<x86_64>::init() noexcept
	{
		/* initialize all handlers to default */
		for (auto &handler: handlers)
			handler = default_handler;

		/* set up exception handlers */
		set_idt_entry(vint_to_vector[EXCEPTION_PAGE_FAULT], reinterpret_cast<void *>(page_fault_handler));
		set_idt_entry(vint_to_vector[EXCEPTION_GENERAL_PROTECTION],
					  reinterpret_cast<void *>(general_protection_handler));
		set_idt_entry(vint_to_vector[EXCEPTION_DOUBLE_FAULT], reinterpret_cast<void *>(double_fault_handler));

		/* set up all other exception gates */
		for (uint8_t i = 0; i <= 20; i++)
		{
			if (i != EXCEPTION_PAGE_FAULT && i != EXCEPTION_GENERAL_PROTECTION && i != EXCEPTION_DOUBLE_FAULT &&
				i != 15)
			{ 
				/* 15 is reserved */
				uint8_t vector = vint_to_vector[i];
				if (i == EXCEPTION_DOUBLE_FAULT || i == EXCEPTION_INVALID_TSS || i == EXCEPTION_SEG_NOT_PRESENT ||
					i == EXCEPTION_STACK_FAULT || i == EXCEPTION_GENERAL_PROTECTION || i == EXCEPTION_PAGE_FAULT ||
					i == EXCEPTION_ALIGNMENT_CHECK)
				{
					/* these exceptions push an error code */
					set_idt_entry(vector, reinterpret_cast<void *>(error_trampoline));
				}
				else
				{
					/* these don't push an error code */
					set_idt_entry(vector, reinterpret_cast<void *>(int_trampoline));
				}
			}
		}

		/* load the idt */
		asm volatile("lidt %0" : : "m"(idt_descriptor));
	}

	void interrupt_traits<x86_64>::enable(uint16_t n) noexcept
	{
		uint8_t x86vector = to_vector(static_cast<Vint>(n));
		if (x86vector >= 32 && x86vector < 48)
		{
			/* TODO: enable the IRQ */
		}
	}

	void interrupt_traits<x86_64>::disable(uint16_t n) noexcept
	{
		uint8_t x86vector = to_vector(static_cast<Vint>(n));
		if (x86vector >= 32 && x86vector < 48)
		{
			/* TODO: mask the IRQ in the pic */
		}
	}

	void interrupt_traits<x86_64>::enable() noexcept
	{
		asm volatile("sti"); /* globally enable interrupts */
	}

	void interrupt_traits<x86_64>::disable() noexcept
	{
		asm volatile("cli"); /* globally disable interrupts */
	}

	void interrupt_traits<x86_64>::register_handler(Vint id, int_handler handler, void *context, uint8_t priority,
													IntFlags flags) noexcept
	{
		uint8_t vector = to_vector(id);

		handlers[vector] = handler;
		handler_contexts[vector] = context;
		handler_flags[vector] = flags;
		handler_priorities[vector] = priority;

		/* if the interrupt is an irq, configure it */
		if (vector >= 32 && vector < 48)
		{
			if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(IntFlags::MASKED)) ==
				static_cast<uint32_t>(IntFlags::MASKED))
			{
				/* TODO: mask the interrupt */
			}
			else if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(IntFlags::UNMASKED)) ==
					 static_cast<uint32_t>(IntFlags::UNMASKED))
			{
				/* TODO: unmask the interrupt */
			}
		}
	}

	uint8_t interrupt_traits<x86_64>::to_vector(Vint id) noexcept
	{
		switch (uint16_t index = static_cast<uint16_t>(id))
		{
			case 0x0000 ... 0x00FF: /* exception interrupts */
				return vint_to_vector[index];

			case 0x0100 ... 0x01FF:							  /* hardware IRQs */
				return vint_to_vector[index - 0x0100 + 0x15]; /* offset in the lookup table */

			case 0x0200: /* syscall - vector 128 (0x80) */
				return 128;

			case 0x1000 ... 0x1FFF:			  /* arch-specific */
				return 48 + (index - 0x1000); /* use vector 48+ for platform-specific */

			case 0x8000 ... 0xFFFF:					  /* user-defined - map to high vectors */
				return 192 + ((index - 0x8000) % 64); /* use vectors 192-255, and maybe some wrap */

			default: /* fallback; sortta */
				return 0;
		}
	}

	Vint interrupt_traits<x86_64>::from_vector(uint8_t vector) noexcept
	{
		return vector_to_vint[vector];
	}
}
