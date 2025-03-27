/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <kafka/heap.hpp>

void* operator new(size_t size)
{
    return kfk::heap::allocate(size);
}

void* operator new[](size_t size)
{
    return kfk::heap::allocate(size);
}

void operator delete(void* ptr) noexcept
{
    kfk::heap::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    kfk::heap::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept
{
    kfk::heap::free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept
{
    kfk::heap::free(ptr);
}