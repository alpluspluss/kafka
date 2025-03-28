/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stddef.h>
#include <string.hpp>

namespace kfk
{
    class Bitmap
    {
    public:
        Bitmap(size_t num_bits) : size(num_bits)
        {
            size_t words = (num_bits + sizeof(size_t) * 8 - 1) / (sizeof(size_t) * 8);
            data = new size_t[words](); /* zero-initialized */
        }

        ~Bitmap()
        {
            delete[] data;
        }

        void set(size_t pos) noexcept
        {
            if (pos >= size) 
                return;

            size_t word = pos / (sizeof(size_t) * 8);
            size_t bit = pos % (sizeof(size_t) * 8);
            data[word] |= (static_cast<size_t>(1) << bit);
        }

        void clear(size_t pos) noexcept
        {
            if (pos >= size) 
                return;
            size_t word = pos / (sizeof(size_t) * 8);
            size_t bit = pos % (sizeof(size_t) * 8);
            data[word] &= ~(static_cast<size_t>(1) << bit);
        }

        bool test(size_t pos) const noexcept
        {
            if (pos >= size) 
                return false;
            size_t word = pos / (sizeof(size_t) * 8);
            size_t bit = pos % (sizeof(size_t) * 8);
            return (data[word] & (static_cast<size_t>(1) << bit)) != 0;
        }

        [[nodiscard]] size_t find_first_set() const noexcept
        {
            size_t words = (size + sizeof(size_t) * 8 - 1) / (sizeof(size_t) * 8);
            for (size_t w = 0; w < words; w++)
            {
                if (data[w] != 0)
                {
                    /* find bit position within word */
                    for (size_t b = 0; b < sizeof(size_t) * 8; b++)
                    {
                        if (data[w] & (static_cast<size_t>(1) << b))
                            return w * sizeof(size_t) * 8 + b;
                    }
                }
            }
            return size;
        }

    private:
        size_t size;
        size_t* data;
    };
}
