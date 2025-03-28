/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <bitmap.hpp>
#include <iostream.hpp>
#include <kafka/fb.hpp>
#include <kafka/hal/cpu.hpp>
#include <kafka/hal/interrupt.hpp>
#include <kafka/hal/vmem.hpp>
#include <kafka/heap.hpp>
#include <kafka/pmem.hpp>
#include <limine.h>
#include <list.hpp>
#include <string.hpp>
#include <unordered_map.hpp>

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
} // namespace

struct TestItem
{
	int value;
	kfk::Node node;

	TestItem(int v) : value(v)
	{}
};

bool test_vmm_mapping()
{
	static constexpr int PAGE_SIZE = 4096;
	uintptr_t mem = kfk::vmm::map_page(5);
	if (!mem)
	{
		kfk::println("Failed to map 5 pages");
		return false;
	}

	kfk::println("Successfully mapped 5 pages at %p", mem);
	auto *ptr = reinterpret_cast<uint32_t *>(mem);
	for (size_t i = 0; i < 5; i++)
	{
		ptr[i * (PAGE_SIZE / sizeof(uint32_t))] = 0xDEADBEEF + i;
		kfk::println("Wrote to page %u at %p", i, ptr + i * (PAGE_SIZE / sizeof(uint32_t)));
	}

	bool success = true;
	for (size_t i = 0; i < 5; i++)
	{
		uint32_t expected = 0xDEADBEEF + i;
		uint32_t actual = ptr[i * (PAGE_SIZE / sizeof(uint32_t))];
		if (actual != expected)
		{
			kfk::println("Verification failed at page %u: expected %x, got %x", i, expected, actual);
			success = false;
		}
	}

	if (success)
		kfk::println("Memory verification successful!");

	kfk::vmm::unmap_page(mem);
	return success;
}

bool test_heap_allocator()
{
	// Initialize the heap if not already done
	kfk::heap::init();

	kfk::println("Testing heap allocator...");

	// Allocate memory for 5 integers
	int *numbers = static_cast<int *>(kfk::heap::allocate(sizeof(int), 5));
	if (!numbers)
	{
		kfk::println("Failed to allocate memory for 5 integers");
		return false;
	}

	kfk::println("Successfully allocated memory for 5 integers at %p", numbers);

	// Write values to the allocated memory
	for (int i = 0; i < 5; i++)
	{
		numbers[i] = 0x1000 + i;
		kfk::println("Wrote %x to index %d at %p", numbers[i], i, &numbers[i]);
	}

	// Verify the values
	bool success = true;
	for (int i = 0; i < 5; i++)
	{
		int expected = 0x1000 + i;
		if (numbers[i] != expected)
		{
			kfk::println("Verification failed at index %d: expected %x, got %x", i, expected, numbers[i]);
			success = false;
		}
	}

	if (success)
	{
		kfk::println("Heap memory verification successful!");
	}

	// Free the memory
	kfk::heap::free(numbers);
	kfk::println("Freed heap memory");

	// Test allocating and freeing different sizes
	void *ptr1 = kfk::heap::allocate(32);
	void *ptr2 = kfk::heap::allocate(64);
	void *ptr3 = kfk::heap::allocate(128);

	if (!ptr1 || !ptr2 || !ptr3)
	{
		kfk::println("Failed to allocate test memory blocks");
		if (ptr1)
			kfk::heap::free(ptr1);
		if (ptr2)
			kfk::heap::free(ptr2);
		if (ptr3)
			kfk::heap::free(ptr3);
		return false;
	}

	kfk::println("Allocated test memory blocks: %p, %p, %p", ptr1, ptr2, ptr3);

	// Write a pattern to each block
	kfk::memset(ptr1, 0xAA, 32);
	kfk::memset(ptr2, 0xBB, 64);
	kfk::memset(ptr3, 0xCC, 128);

	// Free them in different order to test the free list
	kfk::heap::free(ptr2);
	kfk::heap::free(ptr1);
	kfk::heap::free(ptr3);

	kfk::println("Freed test memory blocks");

	return success;
}

void test_operator_new_delete()
{
	kfk::println("Testing operator new and delete...");

	kfk::println("Allocating an integer with new");
	int *p1 = new int(42);
	if (!p1)
	{
		kfk::println("Failed to allocate integer with new");
		return;
	}

	kfk::println("Successfully allocated int at %p with value %d", p1, *p1);

	*p1 = 100;
	kfk::println("Modified value to %d", *p1);

	kfk::println("Deleting integer");
	delete p1;

	kfk::println("Allocating an array with new[]");
	int *arr = new int[10];
	if (!arr)
	{
		kfk::println("Failed to allocate array with new[]");
		return;
	}

	kfk::println("Successfully allocated array at %p", arr);
	for (int i = 0; i < 10; i++)
	{
		arr[i] = i * 10;
		kfk::println("arr[%d] = %d at %p", i, arr[i], &arr[i]);
	}

	kfk::println("Deleting array");
	delete[] arr;

	kfk::println("Allocating a class object with new");
	struct TestClass
	{
		int x;
		int y;

		TestClass() : x(0), y(0)
		{
			kfk::println("TestClass constructor called");
		}

		~TestClass()
		{
			kfk::println("TestClass destructor called");
		}
	};

	TestClass *obj = new TestClass();
	if (!obj)
	{
		kfk::println("Failed to allocate class object with new");
		return;
	}

	kfk::println("Successfully allocated TestClass at %p", obj);

	obj->x = 123;
	obj->y = 456;
	kfk::println("Set x=%d, y=%d", obj->x, obj->y);

	kfk::println("Deleting class object");
	delete obj;

	kfk::println("All new/delete tests completed successfully");
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


	/* early init */
	kfk::fb::init(framebuffer_requests.response->framebuffers[0]);
	kfk::clear();

	if (bool res = kfk::pmm::init(&memmap_request, hhdm_offset); !res)
	{
		kfk::cpu::halt();
	}

	kfk::vmm::init(hhdm_offset);
	kfk::heap::init();

	kfk::cpu::init(hhdm_offset);
	kfk::interrupt::init();

	test_vmm_mapping();
	test_heap_allocator();
    test_operator_new_delete();

	kfk::cpu::pause();
	kfk::cpu::halt();
}
