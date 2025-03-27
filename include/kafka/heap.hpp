/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <kafka/slub.hpp>

namespace kfk
{
    class Heap
    {
    public:
        static void init()
        {
            Slub::init();
        }

        static void* allocate(size_t size, size_t n = 1)
        {
            if (n == 0)
                return nullptr;

            /* overflow!!!!!! */
            if (size > 0 && SIZE_MAX / size < n)
                return nullptr;

            return Slub::allocate(size * n);
        }

        static void free(void* ptr)
        {
            Slub::free(ptr);
        }
    };
}
