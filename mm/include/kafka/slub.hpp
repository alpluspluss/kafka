/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kafka/hal/vmem.hpp>

namespace kfk
{
    class SlubCache;
    
    class Slub
    {
    public:
        static void init();
        
        static void* allocate(size_t size);

        static void free(void* ptr);
        
        static SlubCache* get_cache_for_size(size_t size);
        
    private:
        static void init_small_caches();

        static void init_medium_caches();

        static void init_large_caches();
    };
    
    /* obj metadata */
    struct SlubObject
    {
        uint32_t magic;          /* magic value for validation */
        SlubObject* next_free;   /* ptr to next free object */
    };
    
    /* individual slub */
    struct SlubSlab
    {
        SlubSlab* next; /* next slab in the list */
        size_t obj_size; /* size of each object */
        size_t total_objects; /* total number of objects in slab */
        size_t free_objects; /* number of free objects remaining */
        SlubObject* free_list; /* linked list for free objects */
        void* memory; /* ptr to the slab's memory area */
    };
    
    /* cache of objects of a specific size */
    class SlubCache
    {
    public:
        SlubCache(size_t object_size, size_t pages_per_slab);
        
        SlubCache() 
            : obj_size(0), slab_size(0), slabs(nullptr) {};
        
        void* allocate(size_t n = 1);
        
        bool free(void* ptr);
        
        [[nodiscard]] size_t get_object_size() const;

    private:
        size_t obj_size; /* size of objects in this cache */
        size_t slab_size; /* size of each slab */
        SlubSlab* slabs; /* list of slabs in this cache */
        
        SlubSlab* create_slab();
    };
}
