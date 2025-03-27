/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <kafka/slub.hpp>
#include <kafka/hal/vmem.hpp>

namespace kfk
{
	namespace
	{
		void *memset(void *s, const int c, size_t count)
		{
			auto *xs = static_cast<uint8_t *>(s);
			const auto b = static_cast<uint8_t>(c);
			if (count < 8)
			{
				while (count--)
					*xs++ = b;
				return s;
			}

			/* align to word boundary */
			size_t align = -reinterpret_cast<uintptr_t>(xs) & (sizeof(size_t) - 1);
			count -= align;
			while (align--)
				*xs++ = b;

			/* make word-sized pattern */
			size_t pattern = (b << 24) | (b << 16) | (b << 8) | b;
			pattern = (pattern << 32) | pattern;

			auto *xw = reinterpret_cast<size_t *>(xs);
			while (count >= sizeof(size_t))
			{
				*xw++ = pattern;
				count -= sizeof(size_t);
			}

			/* fill the remaining bytes */
			xs = reinterpret_cast<uint8_t *>(xw);
			while (count--)
				*xs++ = b;

			return s;
		}
	}

	static constexpr size_t SMALL_SIZES_COUNT = 8;
	static constexpr size_t MEDIUM_SIZES_COUNT = 8;
	static constexpr size_t LARGE_SIZES_COUNT = 6;

	static constexpr size_t MAX_SMALL_SIZE = 128;
	static constexpr size_t MAX_MEDIUM_SIZE = 2048;
	static constexpr size_t MAX_LARGE_SIZE = 32768;

	static constexpr size_t SMALL_SLAB_PAGES = 1;  /* 4KB */
	static constexpr size_t MEDIUM_SLAB_PAGES = 4; /* 16KB */
	static constexpr size_t LARGE_SLAB_PAGES = 16; /* 64KB */

	static constexpr size_t PAGE_SIZE = 4096;
	static constexpr uint32_t SLAB_MAGIC = 0x5B5B5B5B;

	static SlubCache small_caches_storage[SMALL_SIZES_COUNT];
	static SlubCache medium_caches_storage[MEDIUM_SIZES_COUNT];
	static SlubCache large_caches_storage[LARGE_SIZES_COUNT];

	static uint8_t slab_buffer[64 * 1024];
	static size_t slab_buffer_offset = 0;

	/* pointers to caches */
	SlubCache *small_caches[SMALL_SIZES_COUNT] = { nullptr };
	SlubCache *medium_caches[MEDIUM_SIZES_COUNT] = { nullptr };
	SlubCache *large_caches[LARGE_SIZES_COUNT] = { nullptr };
	bool initialized = false;

	static void *alloc_from_buffer(size_t size)
	{
		/* align to 8 bytes */
		size = (size + 7) & ~7;

		if (slab_buffer_offset + size > sizeof(slab_buffer))
			return nullptr; /* out of static memory */

		void *ptr = &slab_buffer[slab_buffer_offset];
		slab_buffer_offset += size;

		/* zero the memory */
		memset(ptr, 0, size);
		return ptr;
	}

	size_t SlubCache::get_object_size() const
	{
		return obj_size;
	}

	void Slub::init()
	{
		if (initialized)
			return;

		init_small_caches();
		init_medium_caches();
		init_large_caches();

		initialized = true;
	}

	void Slub::init_small_caches()
	{
		size_t sizes[SMALL_SIZES_COUNT] = { 16, 32, 48, 64, 80, 96, 112, 128 };

		for (size_t i = 0; i < SMALL_SIZES_COUNT; i++)
		{
			small_caches[i] = &small_caches_storage[i];
			*small_caches[i] = SlubCache(sizes[i], SMALL_SLAB_PAGES);
		}
	}

	void Slub::init_medium_caches()
	{
		size_t sizes[MEDIUM_SIZES_COUNT] = { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 };

		for (size_t i = 0; i < MEDIUM_SIZES_COUNT; i++)
		{
			medium_caches[i] = &medium_caches_storage[i];
			*medium_caches[i] = SlubCache(sizes[i], MEDIUM_SLAB_PAGES);
		}
	}

	void Slub::init_large_caches()
	{
		size_t sizes[LARGE_SIZES_COUNT] = { 4096, 8192, 12288, 16384, 24576, 32768 };

		for (size_t i = 0; i < LARGE_SIZES_COUNT; i++)
		{
			large_caches[i] = &large_caches_storage[i];
			*large_caches[i] = SlubCache(sizes[i], LARGE_SLAB_PAGES);
		}
	}

	SlubCache *Slub::get_cache_for_size(size_t size)
	{
		if (size <= MAX_SMALL_SIZE)
		{
			for (size_t i = 0; i < SMALL_SIZES_COUNT; i++)
				if (small_caches[i]->get_object_size() >= size)
					return small_caches[i];

			return small_caches[SMALL_SIZES_COUNT - 1];
		}

		if (size <= MAX_MEDIUM_SIZE)
		{
			for (size_t i = 0; i < MEDIUM_SIZES_COUNT; i++)
				if (medium_caches[i]->get_object_size() >= size)
					return medium_caches[i];

			return medium_caches[MEDIUM_SIZES_COUNT - 1];
		}

		if (size <= MAX_LARGE_SIZE)
		{
			for (size_t i = 0; i < LARGE_SIZES_COUNT; i++)
				if (large_caches[i]->get_object_size() >= size)
					return large_caches[i];

			return large_caches[LARGE_SIZES_COUNT - 1];
		}

		/* size too large for SLUB caches */
		return nullptr;
	}

	void *Slub::allocate(size_t size)
	{
		if (!initialized)
			init();

		/* special case for zero-size allocation */
		if (size == 0)
			size = 1;

		/* for very large allocations, use direct page mapping */
		if (size > MAX_LARGE_SIZE)
		{
			/* calculate needed pages (round up) */
			size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

			/* map pages with kernel read-write permissions */
			uintptr_t memory = vmm::map_page(pages);
			if (memory == 0)
				return nullptr;

			return reinterpret_cast<void *>(memory);
		}

		/* get the appropriate cache */
		SlubCache *cache = get_cache_for_size(size);
		if (!cache)
			return nullptr;

		/* allocate from the cache */
		return cache->allocate();
	}

	void Slub::free(void *ptr)
	{
		if (!ptr)
			return;

		/* try to free in each cache */
		for (size_t i = 0; i < SMALL_SIZES_COUNT; i++)
			if (small_caches[i]->free(ptr))
				return;

		for (size_t i = 0; i < MEDIUM_SIZES_COUNT; i++)
			if (medium_caches[i]->free(ptr))
				return;

		for (size_t i = 0; i < LARGE_SIZES_COUNT; i++)
			if (large_caches[i]->free(ptr))
				return;

		/* might be a direct page allocation if not found. hopefully... */
		vmm::unmap_page(reinterpret_cast<uintptr_t>(ptr));
	}

	SlubCache::SlubCache(size_t object_size, size_t pages_per_slab) :
		obj_size(object_size), slab_size(pages_per_slab * PAGE_SIZE), slabs(nullptr)
	{
		/* ensure object size is large enough to hold freelist pointer */
		if (obj_size < sizeof(SlubObject))
			obj_size = sizeof(SlubObject);
	}

	SlubSlab *SlubCache::create_slab()
	{
		SlubSlab *slab = static_cast<SlubSlab *>(alloc_from_buffer(sizeof(SlubSlab)));
		if (!slab)
			return nullptr;

		/* map memory for the slab */
		size_t pages = slab_size / PAGE_SIZE;
		uintptr_t memory = vmm::map_page(pages);
		if (memory == 0)
			return nullptr;

		/* init slab descriptor */
		slab->memory = reinterpret_cast<void *>(memory);
		slab->obj_size = obj_size;
		slab->total_objects = (slab_size / obj_size);
		slab->free_objects = slab->total_objects;
		slab->free_list = nullptr;
		slab->next = nullptr;

		for (size_t i = 0; i < slab->total_objects; i++) /* objs & metadata init */
		{
			uintptr_t obj_addr = memory + (i * obj_size);
			SlubObject *obj = reinterpret_cast<SlubObject *>(obj_addr);

			obj->magic = SLAB_MAGIC;
			obj->next_free = slab->free_list;

			/* add to free list */
			slab->free_list = obj;
		}

		return slab;
	}

	void *SlubCache::allocate(size_t n)
	{
		size_t total_size = n * obj_size;
		if (n > 1 && total_size > obj_size)
			/* too big for this cache */
			return Slub::allocate(total_size);

		SlubSlab *slab = slabs;
		SlubSlab *prev = nullptr;

		while (slab && slab->free_objects == 0)
		{
			prev = slab;
			slab = slab->next;
		}

		/* create a new one if no slab with free objects found */
		if (!slab)
		{
			slab = create_slab();
			if (!slab)
				return nullptr;

			/* add to slab list */
			if (prev)
				prev->next = slab;
			else
				slabs = slab;
		}

		/* get object from free list */
		SlubObject *obj = slab->free_list;
		slab->free_list = obj->next_free;
		slab->free_objects--;

		/* zero-out; security reasons */
		memset(obj, 0, obj_size);
		return obj;
	}

	bool SlubCache::free(void *ptr)
	{
		/* check if this object belongs to any of our slabs */
		for (SlubSlab *slab = slabs; slab; slab = slab->next)
		{
			uintptr_t slab_start = reinterpret_cast<uintptr_t>(slab->memory);
			uintptr_t slab_end = slab_start + slab_size;
			uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);

			if (ptr_addr >= slab_start && ptr_addr < slab_end) /* in the slab? */
			{
				if ((ptr_addr - slab_start) % obj_size != 0) /* check alignment */
					return false;

				/* add to freelist */
				SlubObject *obj = reinterpret_cast<SlubObject*>(ptr);
				obj->magic = SLAB_MAGIC;
				obj->next_free = slab->free_list;
				slab->free_list = obj;
				slab->free_objects++;
				return true;
			}
		}

		return false; /* not found */
	}
}
