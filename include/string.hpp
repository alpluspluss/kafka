/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace kfk
{
    inline void* memset(void *s, const int c, size_t count)
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

    inline void* memcpy(void *dest, const void *src, size_t count)
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

    inline void* memmove(void *dest, const void *src, size_t count)
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

    static int memcmp(const void *cs, const void *ct, size_t count)
    {
        const auto *s1 = static_cast<const uint8_t *>(cs);
        const auto *s2 = static_cast<const uint8_t *>(ct);
        if (count < 8)
        {
            while (count--)
            {
                if (*s1 != *s2)
                    return *s1 - *s2;
                s1++;
                s2++;
            }
            return 0;
        }

        size_t align = -reinterpret_cast<uintptr_t>(s1) & (sizeof(size_t)-1);
        count -= align;
        while (align--)
        {
            if (*s1 != *s2)
                return *s1 - *s2;
            s1++;
            s2++;
        }

        const auto *w1 = reinterpret_cast<const size_t *>(s1);
        const auto *w2 = reinterpret_cast<const size_t *>(s2);
        while (count >= sizeof(size_t))
        {
            if (*w1 != *w2)
            {
                s1 = reinterpret_cast<const uint8_t *>(w1);
                s2 = reinterpret_cast<const uint8_t *>(w2);
                for (size_t i = 0; i < sizeof(size_t); i++)
                {
                    if (s1[i] != s2[i])
                        return s1[i] - s2[i];
                }
            }
            w1++;
            w2++;
            count -= sizeof(size_t);
        }

        s1 = reinterpret_cast<const uint8_t *>(w1);
        s2 = reinterpret_cast<const uint8_t *>(w2);
        while (count--)
        {
            if (*s1 != *s2)
                return *s1 - *s2;
            s1++;
            s2++;
        }

        return 0;
    }
}
