/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <stdint.h>
#include <kafka/X86cpu.hpp>
#include <kafka/gdt.hpp>
#include <kafka/tss.hpp>
#include <kafka/types.hpp>

namespace kfk
{
    /* static & const variables */
    static GDTEntry gdt[] = {
        { 0, 0, 0, 0, 0, 0 },                /* null descriptor */
		{ 0xFFFF, 0, 0, 0x9A, 0xF, 0xA, 0 }, /* kernel code */
		{ 0xFFFF, 0, 0, 0x92, 0xF, 0xA, 0 }, /* kernel data */
		{ 0, 0, 0, 0, 0, 0 },                /* tss low */
		{ 0, 0, 0, 0, 0, 0 }                 /* tss high */
    };

    static GDTDescriptor gdtr = {
		sizeof(gdt) - 1,
		reinterpret_cast<uint64_t>(gdt)
	};

    alignas(16) static TSS tss = {};

    /* MSR stuff */
    static constexpr uint64_t TSS_LIMIT = sizeof(TSS);
	static constexpr auto MSR_EFER = 0xC0000080;
	static constexpr auto MSR_STAR = 0xC0000081;
	static constexpr auto MSR_LSTAR = 0xC0000082;
	static constexpr auto MSR_SYSCALL_MASK = 0xC0000084;
	static constexpr auto MSR_GS_BASE = 0xC0000101;
	static constexpr auto MSR_KERNEL_GS_BASE = 0xC0000102;

    /* variables */
    static uint64_t hhdm_offset = 0;

	static void syscall_entry() __attribute__((naked));

	static void syscall_entry()
	{
		asm volatile(
			"swapgs\n"
			"movq %rsp, %gs:16\n"
			"movq %gs:8, %rsp\n"
			"ret"
		);
	}

    void cpu_traits<x86_64>::init(uint64_t offset) noexcept
    {
        /* save hhdm offset */
        hhdm_offset = offset;

        /* load GDT */
        asm volatile("lgdt %0" : : "m"(gdtr)); /* flush */
		asm volatile(
			"pushq $0x08\n"	/* code segment selector */
			"pushq $1f\n"  	/* return address */
			"lretq\n"
			"1:\n"
		);
		asm volatile(
			"mov $0x10, %%ax\n"
			"mov %%ax, %%ds\n"
			"mov %%ax, %%es\n"
			"mov %%ax, %%fs\n"
			"mov %%ax, %%gs\n"
			"mov %%ax, %%ss\n"
			: : : "ax"
		);

        /* TSS */
        const auto tss_base = reinterpret_cast<uint64_t>(&tss); /* get the address */

        /* generally TSS descriptor is 16 bytes which is split across 2 GDT entries */
		/* first entry [LOW] */
		gdt[3].limit_low = TSS_LIMIT & 0xFFFF;
		gdt[3].base_low = tss_base & 0xFFFF;
		gdt[3].base_middle = (tss_base >> 16) & 0xFF;
		gdt[3].access = 0x89; /* PRESENT | RING0 | TSS */
		gdt[3].limit_high = (TSS_LIMIT >> 16) & 0xF;
		gdt[3].flags = 0;
		gdt[3].base_high = (tss_base >> 24) & 0xFF;

		/* second entry [HIGH]; see GDT definitions for more info */
		gdt[4].limit_low = (tss_base >> 32) & 0xFFFF;
		gdt[4].base_low = (tss_base >> 48) & 0xFFFF;
		gdt[4].base_middle = 0;
		gdt[4].access = 0;
		gdt[4].limit_high = 0;
		gdt[4].flags = 0;
		gdt[4].base_high = 0;

		/* load TSS */
		tss.iopb = sizeof(TSS);
		asm volatile(
			"ltr %%ax"
			: : "a"(0x18) /* according to OSDev wiki; it is at 0x18 (3 * 8) */
		);

		/* enable syscall and sysret */
		uint64_t efer = rdmsr(MSR_EFER);
		efer |= 1ULL << 0;
		wrmsr(MSR_EFER, efer);

		/* STAR & LSTAR setup */
		uint64_t star = (0x13ULL << 48) | (0x08ULL << 32);
		wrmsr(MSR_STAR, star);
		
		wrmsr(MSR_LSTAR, reinterpret_cast<uint64_t>(+syscall_entry));
		wrmsr(MSR_SYSCALL_MASK, 0x200);

		/* GC base because this is IMPORTANT to separate kernel & userspace */
		wrmsr(MSR_GS_BASE, 0);
		wrmsr(MSR_KERNEL_GS_BASE, 0);
    }

	[[noreturn]] void cpu_traits<x86_64>::halt() noexcept
	{
		while (true)
			asm volatile("hlt");
	}

	uint64_t cpu_traits<x86_64>::rdmsr(uint32_t msr) noexcept
	{
		uint32_t low, high;
		asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
		return (static_cast<uint64_t>(high) << 32) | low;
	}

	void cpu_traits<x86_64>::wrmsr(uint32_t msr, uint64_t value) noexcept
	{
		uint32_t low = value & 0xFFFFFFFF;
		uint32_t high = value >> 32;
		asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
	}

	void cpu_traits<x86_64>::invlpg(void *addr) noexcept
	{
		asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
	}

	void cpu_traits<x86_64>::ltr(uint16_t selector) noexcept
	{
		asm volatile("ltr %0" : : "r"(selector));
	}

	void cpu_traits<x86_64>::cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx,
										uint32_t *ecx,
										uint32_t *edx) noexcept
	{
		asm volatile("cpuid"
			: "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
			: "a"(leaf), "c"(subleaf)
		);
	}

	uint64_t cpu_traits<x86_64>::read_cr0() noexcept
	{
		uint64_t value;
		asm volatile("mov %%cr0, %0" : "=r"(value));
		return value;
	}

	uint64_t cpu_traits<x86_64>::read_cr2() noexcept
	{
		uint64_t value;
		asm volatile("mov %%cr2, %0" : "=r"(value));
		return value;
	}

	uint64_t cpu_traits<x86_64>::read_cr3() noexcept
	{
		uint64_t value;
		asm volatile("mov %%cr3, %0" : "=r"(value));
		return value;
	}

	uint64_t cpu_traits<x86_64>::read_cr4() noexcept
	{
		uint64_t value;
		asm volatile("mov %%cr4, %0" : "=r"(value));
		return value;
	}

	void cpu_traits<x86_64>::write_cr0(uint64_t value) noexcept
	{
		asm volatile("mov %0, %%cr0" : : "r"(value));
	}

	void cpu_traits<x86_64>::write_cr3(uint64_t value) noexcept
	{
		asm volatile("mov %0, %%cr3" : : "r"(value));
	}

	void cpu_traits<x86_64>::write_cr4(uint64_t value) noexcept
	{
		asm volatile("mov %0, %%cr4" : : "r"(value));
	}

	uint64_t cpu_traits<x86_64>::xgetbv(uint32_t xcr) noexcept
	{
		uint32_t eax, edx;
		asm volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
		return (static_cast<uint64_t>(edx) << 32) | eax;
	}

	void cpu_traits<x86_64>::xsetbv(uint32_t xcr, uint64_t value) noexcept
	{
		uint32_t eax = value & 0xFFFFFFFF;
		uint32_t edx = value >> 32;
		asm volatile("xsetbv" : : "a"(eax), "d"(edx), "c"(xcr));
	}

	void cpu_traits<x86_64>::pause() noexcept
	{
		asm volatile("pause");
	}
}