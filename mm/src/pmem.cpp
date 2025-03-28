/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <iostream.hpp>
#include <kafka/pmem.hpp>

namespace kfk
{
    namespace /* to be moved to <string.hpp> */
    {
        void* memcpy(void *dest, const void *src, size_t count)
        {
            auto *d = static_cast<uint8_t *>(dest);
            const auto *s = static_cast<const uint8_t *>(src);

            if (count < 8 || ((reinterpret_cast<uintptr_t>(d) | reinterpret_cast<uintptr_t>(s)) & (sizeof(size_t) - 1)))
            {
                while (count--)
                    *d++ = *s++;
                return dest;
            }

            auto *dw = reinterpret_cast<size_t *>(d);
            const auto *sw = reinterpret_cast<const size_t *>(s);
            while (count >= sizeof(size_t))
            {
                *dw++ = *sw++;
                count -= sizeof(size_t);
            }

            d = reinterpret_cast<uint8_t *>(dw);
            s = reinterpret_cast<const uint8_t *>(sw);
            while (count--)
                *d++ = *s++;

            return dest;
        }
        void* memmove(void *dest, const void *src, size_t count)
        {
            auto *d = static_cast<uint8_t *>(dest);
            const auto *s = static_cast<const uint8_t *>(src);

            if (d == s || count == 0)
                return dest;

            /* forward copy if they don't overlap */
            if (d < s || d >= s + count)
            {
                return memcpy(dest, src, count);
            }

            /* copy backward to avoid corruption otherwise */
            d += count - 1;
            s += count - 1;

            if (count < 8)
            {
                while (count--)
                    *d-- = *s--;
                return dest;
            }

            /* handle misaligned bytes */
            size_t align = (reinterpret_cast<uintptr_t>(d) + 1) & (sizeof(size_t) - 1);
            count -= align;
            while (align--)
                *d-- = *s--;

            /* word-by-word backwad */
            auto *dw = reinterpret_cast<size_t *>(d - sizeof(size_t) + 1);
            const auto *sw = reinterpret_cast<const size_t *>(s - sizeof(size_t) + 1);
            while (count >= sizeof(size_t))
            {
                *dw-- = *sw--;
                count -= sizeof(size_t);
            }

            d = reinterpret_cast<uint8_t *>(dw + 1);
            s = reinterpret_cast<const uint8_t *>(sw + 1);
            while (count--)
                *d-- = *s--;

            return dest;
        }
    }

    struct Region
    {
        uintptr_t base;
        size_t len;
        bool is_free; /* may layer be a uint8_t flags instead */
    };

    static constexpr size_t PAGE_SIZE = 4096;
	static constexpr size_t MAX_REGIONS = 64;

    static uint64_t hhdm_offset = 0;
	Region regions[MAX_REGIONS];
	size_t region_count = 0;

    bool PhysicalPageManager::init(volatile limine_memmap_request *mmap, uint64_t offset) noexcept
    {
        const limine_memmap_response* response = mmap->response;
        if (!mmap->response)
            return false; /* cooked */

        hhdm_offset = offset;
        for (size_t i = 0; i < response->entry_count && region_count < MAX_REGIONS; i++)
        {
            if (const auto entry = mmap->response->entries[i];
                entry->type == LIMINE_MEMMAP_USABLE)
            {
                /* align base and len with page boundaries */
                const uintptr_t base = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                if (const uintptr_t end = (entry->base + entry->length) & ~(PAGE_SIZE - 1);
					end > base)
				{
					regions[region_count++] = {
						.base = base,
						.len = end - base,
						.is_free = true
					};
				}
            }
        }

        for (size_t i = 1; i < region_count; i++)
		{
			const Region key = regions[i];
			size_t j = i;
			while (j > 0 && regions[j - 1].base > key.base)
			{
				regions[j] = regions[j - 1];
				j--;
			}
			regions[j] = key;
		}

		/* merge all regions that are adjacent */
		for (size_t i = 0; i < region_count - 1; i++)
		{
			if (regions[i].base + regions[i].len == regions[i + 1].base)
			{
				regions[i].len += regions[i + 1].len;
				memmove(&regions[i + 1], &regions[i + 2], (region_count - i - 2) * sizeof(Region));
				region_count--;
				i--;
			}
		}

        // for (size_t i = 0; i < region_count; ++i)
        // {
        //     printf("  Region ");
		// 	printf("%d", i);
		// 	printf(": Base=%x", regions[i].base);
		// 	printf(" Len=%x", regions[i].len);
		// 	print(" ");
		// 	println(regions[i].is_free ? "Free" : "Used");
        // }

        return true;
    }

    uintptr_t PhysicalPageManager::pmalloc(const uint64_t n) noexcept
    {
        /* for the best cache efficiency, find best-fit */
        const size_t size = n * PAGE_SIZE;
        kfk::println();
        size_t best_index = region_count;
        uint64_t smallest = UINT64_MAX;

        for (size_t i = 0; i < region_count; ++i)
		{
			if (regions[i].is_free && regions[i].len >= size)
			{
				if (regions[i].len < smallest)
				{
					smallest = regions[i].len;
					best_index = i;
				}
			}
		}

        if (best_index < region_count)
        {
            const uintptr_t alloc_base = regions[best_index].base;
            if (regions[best_index].len == size)
            {
                regions[best_index].is_free = false;
            }
            else 
            {
                if (region_count >= MAX_REGIONS)
                    return 0;

                /* shift regions to make space for the new split region */
				memmove(&regions[best_index + 2], &regions[best_index + 1],
                    (region_count - best_index - 1) * sizeof(Region));

                regions[best_index + 1].base = regions[best_index].base + size;
                regions[best_index + 1].len = regions[best_index].len - size;
                regions[best_index + 1].is_free = true;
    
                regions[best_index].len = size;
                regions[best_index].is_free = false;
                region_count++;
            }

            /* `CACHE_LINE` should be elsewhere; maybe use <kafka/hwinfo> to query once the header is completed */
            constexpr size_t CACHE_LINE = 64;
            auto *ptr = reinterpret_cast<volatile uint8_t *>(alloc_base + hhdm_offset);
            for (size_t offset = 0; offset < size; offset += CACHE_LINE)
            {
                for (size_t i = 0; i < CACHE_LINE && offset + i < size; i++)
                    ptr[offset + i] = 0;
            }
            return alloc_base;
        }

        return 0;
    }

    void PhysicalPageManager::pfree(uintptr_t base, const uint64_t n) noexcept
    {
        const size_t size = n * PAGE_SIZE;

        /* find the region containing this base address */
        /* TODO: switch to kfk::lower_bound(); once we have the kernel std */
		size_t left = 0, right = region_count - 1;
		size_t i = region_count;
		while (left <= right)
		{
			const size_t mid = left + (right - left) / 2;
			if (regions[mid].base == base)
			{
				i = mid;
				break;
			}
			if (regions[mid].base < base)
			{
				left = mid + 1;
			}
			else
			{
				right = mid - 1;
			}
		}

        if (i < region_count && !regions[i].is_free)
        {
            /* exact size match: mark entire region as free */
            if (regions[i].len == size)
            {
                regions[i].is_free = true;
            }
            
            else if (size < regions[i].len) /* partial freeing */
            {
                /* check if we have space for a new region */
                if (region_count >= MAX_REGIONS)
                    return;
                    
                /* make room for a new region */
                memmove(&regions[i + 1], &regions[i], (region_count - i) * sizeof(Region));
                region_count++;
                
                /* split the region */
                regions[i + 1].base = base + size;
                regions[i + 1].len = regions[i].len - size;
                regions[i + 1].is_free = false;
                
                regions[i].len = size;
                regions[i].is_free = true;
            }
            else /* ignore; trying to free more than what's allocated */
            {
                return;
            }
            
            /* try to merge with adjacent free regions */
            if (i > 0 && regions[i - 1].is_free) /* merge with prev */
            {
                regions[i - 1].len += regions[i].len;
                memmove(&regions[i], &regions[i + 1], (region_count - i - 1) * sizeof(Region));
                region_count--;
                i--;
            }

            if (i < region_count - 1 && regions[i + 1].is_free) /* merge with next */
            {
                regions[i].len += regions[i + 1].len;
                memmove(&regions[i + 1], &regions[i + 2], (region_count - i - 2) * sizeof(Region));
                region_count--;
            }
        }
    }
}
