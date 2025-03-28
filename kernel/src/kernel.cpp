/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <limine.h>
#include <bitmap.hpp>
#include <iostream.hpp>
#include <list.hpp>
#include <string.hpp>
#include <unordered_map.hpp>
#include <kafka/fb.hpp>
#include <kafka/heap.hpp>
#include <kafka/pmem.hpp>
#include <kernel/policy.hpp>
#include <kafka/hal/cpu.hpp>
#include <kafka/hal/interrupt.hpp>
#include <kafka/hal/vmem.hpp>

namespace
{
	__attribute__((used, section(".limine_requests"))) volatile LIMINE_BASE_REVISION(3);

	__attribute__((used, section(".limine_requests"))) volatile limine_framebuffer_request framebuffer_requests = {
		.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0, .response = nullptr
	};

	__attribute__((used, section(".limine_requests"))) volatile limine_hhdm_request hhdm_request = {
		.id = LIMINE_HHDM_REQUEST, .revision = 0, .response = nullptr
	};

	__attribute__((used, section(".limine_requests"))) volatile limine_memmap_request memmap_request = {
		.id = LIMINE_MEMMAP_REQUEST, .response = nullptr
	};

	__attribute__((used, section(".limine_requests_start"))) volatile LIMINE_REQUESTS_START_MARKER;

	__attribute__((used, section(".limine_requests_end"))) volatile LIMINE_REQUESTS_END_MARKER;
}

extern "C" void kernel_main()
{
	if (LIMINE_BASE_REVISION_SUPPORTED == false)
		kfk::cpu::halt();

	if (framebuffer_requests.response == nullptr || framebuffer_requests.response->framebuffer_count < 1)
	{
		kfk::cpu::halt();
	}

	uint64_t hhdm_offset = hhdm_request.response->offset;
	if (!hhdm_offset)
		kfk::cpu::halt();

	/* for debug */
	kfk::fb::init(framebuffer_requests.response->framebuffers[0]);
	kfk::clear();

	/* bootstrap */
	if (bool res = kfk::pmm::init(&memmap_request, hhdm_offset); !res)
		kfk::cpu::halt();

	kfk::vmm::init(hhdm_offset);
	kfk::heap::init();

	/* use dynamic allocation policy */
	policy::dynamic_alloc();

	kfk::cpu::init(hhdm_offset);
	kfk::interrupt::init();

	kfk::cpu::pause();
	kfk::cpu::halt();
}
