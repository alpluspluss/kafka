/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <type_traits.hpp>

namespace kfk
{
    template<typename T>
    constexpr typename remove_reference<T>::type&& move(T&& v) noexcept
    {
        return static_cast<typename remove_reference<T>::type&&>(v);
    }

    /* lvalue references forward */
    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type& t) noexcept
    {
        return static_cast<T&&>(t);
    }

    /* rvalue references forward */
    template<typename T>
    constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept
    {
        static_assert(!is_lvalue_reference<T>::value, 
                     "cannot forward an rvalue as an lvalue");
        return static_cast<T&&>(t);
    }

    template<typename T>
    constexpr void swap(T& a, T& b) noexcept
    {
        T temp = static_cast<T&&>(a);
        a = static_cast<T&&>(b);
        b = static_cast<T&&>(temp);
    }

    template<typename T>
    uint64_t hash(const T& value)
    {
        static constexpr uint64_t FNV_PRIME = 1099511628211UL;
        static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037UL;

        uint64_t hash_value = FNV_OFFSET_BASIS;
        auto bytes = static_cast<const unsigned char*>(&value);
        for (uint64_t i = 0; i < sizeof(T); ++i)
        {
            hash_value ^= bytes[i];
            hash_value *= FNV_PRIME;
        }
        
        return hash_value;
    }
    
    inline uint64_t hash(const char* str)
    {
        static constexpr uint64_t FNV_PRIME = 1099511628211UL;
        static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037UL;
        
        uint64_t hash_value = FNV_OFFSET_BASIS;
        
        while (*str)
        {
            hash_value ^= static_cast<uint8_t>(*str);
            hash_value *= FNV_PRIME;
            ++str;
        }
        
        return hash_value;
    }

    inline uint64_t hash(int8_t value)
    {
        return static_cast<uint64_t>(value) * 2654435761UL;
    }

    inline uint64_t hash(uint8_t value)
    {
        return static_cast<uint64_t>(value) * 2654435761UL;
    }

    inline uint64_t hash(int16_t value)
    {
        uint32_t x = static_cast<uint32_t>(value);
        x = ((x >> 8) ^ x) * 0x27d4eb2d;
        return static_cast<uint64_t>(x);
    }

    inline uint64_t hash(uint16_t value)
    {
        uint32_t x = static_cast<uint32_t>(value);
        x = ((x >> 8) ^ x) * 0x27d4eb2d;
        return static_cast<uint64_t>(x);
    }

    inline uint64_t hash(int32_t value)
    {
        uint32_t x = static_cast<uint32_t>(value);
        x = ((x >> 16) ^ x) * 0x85ebca6b;
        x = ((x >> 13) ^ x) * 0xc2b2ae35;
        x = (x >> 16) ^ x;
        return static_cast<uint64_t>(x);
    }

    inline uint64_t hash(uint32_t value)
    {
        uint32_t x = value;
        x = ((x >> 16) ^ x) * 0x85ebca6b;
        x = ((x >> 13) ^ x) * 0xc2b2ae35;
        x = (x >> 16) ^ x;
        return static_cast<uint64_t>(x);
    }

    inline uint64_t hash(int64_t value)
    {
        uint64_t x = static_cast<uint64_t>(value);
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9UL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebUL;
        x = x ^ (x >> 31);
        return x;
    }

    inline uint64_t hash(uint64_t value)
    {
        uint64_t x = value;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9UL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebUL;
        x = x ^ (x >> 31);
        return x;
    }

    template<typename T>
    uint64_t hash(T* ptr)
    {
        uintptr_t x = reinterpret_cast<uintptr_t>(ptr);
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9UL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebUL;
        x = x ^ (x >> 31);
        return static_cast<uint64_t>(x);
    }

    inline uint64_t hash(bool value)
    {
        return value ? 1 : 0;
    }

    inline uint64_t hash(char value)
    {
        return static_cast<uint64_t>(value) * 2654435761UL;
    }
}
