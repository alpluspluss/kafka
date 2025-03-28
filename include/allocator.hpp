/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <atomic.hpp>
#include <string.hpp>
#include <kafka/slub.hpp>

namespace kfk
{
    enum class AllocPolicy
    {
        STATIC, /* always static */
        DYNAMIC, /* always dynamic (post-init )*/
        SWITCHABLE /* switchable */
    };

    struct AllocHeader
    {
        static constexpr uint32_t MAGIC = 0xA110CA7E; /* "ALLOCATE" */
        AllocHeader* next; /* for linked list */
        AllocHeader* prev; /* for linked list */
        size_t size; /* size of allocation no including header */
        uint32_t magic; /* for validation */
        bool is_dynamic; /* whether this is a dynamic allocation */
    };

    /* primary template for the allocator; each policy has specialization for optimal effiency */
    template<AllocPolicy Policy = AllocPolicy::SWITCHABLE, size_t StaticSize = 128 * 1024>
    class Allocator;

    template<size_t StaticSize>
    class Allocator<AllocPolicy::STATIC, StaticSize>
    {
    public:
        constexpr Allocator() : static_offset(0), static_allocs(nullptr)
        {
            for (size_t i = 0; i < StaticSize; ++i)
                buffer[i] = 0;
        }

        void* malloc(size_t size) noexcept
        {
            if (size == 0)
                size = 1;

            const size_t total_size = size + sizeof(AllocHeader);

            /* align the offset */
            static_offset = (static_offset + alignof(max_align_t) - 1) 
                          & ~(alignof(max_align_t) - 1);

            if (static_offset + total_size > StaticSize)
                return nullptr; /* out of static memory */
            
            void* mem = &buffer[static_offset];
            static_offset += total_size;

            auto header = static_cast<AllocHeader*>(mem);
            header->size = size;
            header->magic = AllocHeader::MAGIC;
            header->is_dynamic = false;
            
            /* add to linked list of static allocs */
            header->next = static_allocs;
            header->prev = nullptr;
            
            if (static_allocs)
                static_allocs->prev = header;
                
            static_allocs = header;
            return reinterpret_cast<uint8_t*>(mem) + sizeof(AllocHeader);
        }

        void free(void* ptr) noexcept
        {
            if (!ptr)
                return;

            auto header = reinterpret_cast<AllocHeader*>(
                reinterpret_cast<uint8_t*>(ptr) - sizeof(AllocHeader));
            
            if (header->magic != AllocHeader::MAGIC)
                return; /* invalid header */

            if (header->prev)
                header->prev->next = header->next;
            else
                static_allocs = header->next;
                
            if (header->next)
                header->next->prev = header->prev;
                
             /* Zero out for security */
            const size_t total_size = header->size + sizeof(AllocHeader);
            kfk::memset(header, 0, total_size);
        }

    private:
        alignas(alignof(max_align_t)) uint8_t buffer[StaticSize];

        size_t static_offset;
        AllocHeader* static_allocs;
    };

    template<size_t StaticSize>
    class Allocator<AllocPolicy::DYNAMIC, StaticSize>
    {
    public:
        constexpr Allocator() {}

        void* allocate(size_t size) noexcept
        {
            if (size == 0)
                size = 1;

            const size_t total_size = size + sizeof(AllocHeader);
            void* mem = kfk::Slub::allocate(total_size);
            if (!mem)
                return nullptr;
            
            /* setup header */
            AllocHeader* header = static_cast<AllocHeader*>(mem);
            header->next = nullptr;
            header->prev = nullptr;
            header->size = size;
            header->magic = AllocHeader::MAGIC;
            header->is_dynamic = true;
            
            /* return usable memory after header */
            return reinterpret_cast<uint8_t*>(mem) + sizeof(AllocHeader);
        }

        /* free allocated memory */
        void free(void* ptr) noexcept
        {
            if (!ptr)
                return;
            
            /* get the header from the pointer */
            AllocHeader* header = reinterpret_cast<AllocHeader*>(
                reinterpret_cast<uint8_t*>(ptr) - sizeof(AllocHeader));
            
            /* validate header */
            if (header->magic != AllocHeader::MAGIC)
                return; /* Invalid free */
            
            /* dynamic allocations only, pass to SLUB */
            kfk::Slub::free(header);
        }
    };

    template<size_t StaticSize>
    class Allocator<AllocPolicy::SWITCHABLE, StaticSize>
    {
    public:
        constexpr Allocator() : 
            static_offset(0), 
            static_allocs(nullptr),
            instance_dynamic_ready(false)
        {
            /* zero the buffer during construction */
            for (size_t i = 0; i < StaticSize; ++i)
                buffer[i] = 0;
        }

        void use_dynamic() noexcept
        {
            instance_dynamic_ready = true;
        }

        bool is_dynamic_ready() const noexcept
        {
            return instance_dynamic_ready;
        }

        void* allocate(size_t size) noexcept
        {
            if (size == 0)
                size = 1;

            /* add space for the header */
            const size_t total_size = size + sizeof(AllocHeader);

            /* determine allocation strategy based on instance state */
            if (instance_dynamic_ready)
            {
                /* allocate from SLUB */
                void* mem = kfk::Slub::allocate(total_size);
                if (!mem)
                    return nullptr;
                
                /* setup header */
                AllocHeader* header = static_cast<AllocHeader*>(mem);
                header->next = nullptr;
                header->prev = nullptr;
                header->size = size;
                header->magic = AllocHeader::MAGIC;
                header->is_dynamic = true;
                
                /* return usable memory after header */
                return reinterpret_cast<uint8_t*>(mem) + sizeof(AllocHeader);
            }
            else
            {
                /* allocate from static buffer */
                /* align offset to satisfy alignment requirements */
                static_offset = (static_offset + alignof(max_align_t) - 1) 
                              & ~(alignof(max_align_t) - 1);
                              
                /* check if we have enough space */
                if (static_offset + total_size > StaticSize)
                    return nullptr; /* Out of static memory */
                
                /* Get memory from buffer */
                void* mem = &buffer[static_offset];
                static_offset += total_size;
                
                /* setup header */
                AllocHeader* header = static_cast<AllocHeader*>(mem);
                header->size = size;
                header->magic = AllocHeader::MAGIC;
                header->is_dynamic = false;
                
                /* add to linked list of static allocations */
                header->next = static_allocs;
                header->prev = nullptr;
                
                if (static_allocs)
                    static_allocs->prev = header;
                    
                static_allocs = header;
                
                /* Return usable memory after header */
                return reinterpret_cast<uint8_t*>(mem) + sizeof(AllocHeader);
            }
        }
        void free(void* ptr) noexcept
        {
            if (!ptr)
                return;
            
            AllocHeader* header = reinterpret_cast<AllocHeader*>(
                reinterpret_cast<uint8_t*>(ptr) - sizeof(AllocHeader));
            
            /* validate header */
            if (header->magic != AllocHeader::MAGIC)
                return;
            
            if (header->is_dynamic)
            {
                /* dynamic allocation - free through SLUB */
                kfk::Slub::free(header);
            }
            else
            {
                /* static allocation - remove from linked list */
                if (header->prev)
                    header->prev->next = header->next;
                else
                    static_allocs = header->next;
                    
                if (header->next)
                    header->next->prev = header->prev;
                    
                /* zero out for security */
                const size_t total_size = header->size + sizeof(AllocHeader);
                memset(header, 0, total_size);
            }
        }
        
    private:
        alignas(alignof(max_align_t)) uint8_t buffer[StaticSize];
        
        size_t static_offset;
        AllocHeader* static_allocs;
        
        bool instance_dynamic_ready;
    };

    using BootstrapAllocator = Allocator<AllocPolicy::STATIC, 256 * 1024>;
    inline BootstrapAllocator boot_allocator;

    using KernelAllocator = Allocator<AllocPolicy::SWITCHABLE, 512 * 1024>;
    inline KernelAllocator kernel_allocator;

    inline void* allocate(size_t size) noexcept
    {
        return kernel_allocator.allocate(size);
    }
    
    inline void free(void* ptr) noexcept
    {
        kernel_allocator.free(ptr);
    }
}
