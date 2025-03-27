/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <utilities.hpp>

namespace kfk
{
    template<typename T>
    constexpr const T& min(const T& a, const T& b)
    {
        return (b < a) ? b : a;
    }

    template<typename T>
    constexpr const T& max(const T& a, const T& b)
    {
        return (a < b) ? b : a;
    }
    
    template<typename T>
    constexpr T clamp(const T& v, const T& lo, const T& hi)
    {
        return max(lo, min(v, hi));
    }

    template<typename T, typename U>
    constexpr size_t find(const T* arr, size_t size, const U& value)
    {
        for (size_t i = 0; i < size; i++)
        {
            if (arr[i] == value)
                return i;
        }
        return size; /* not found */
    }

    template<typename T, typename U>
    constexpr size_t lower_bound(const T* arr, size_t size, const U& value)
    {
        T* base = const_cast<T*>(arr);
        size_t len = size;
        
        while (len > 1)
        {
            size_t half = len / 2;
            base += (base[half - 1] < value) * half;
            len -= half;
        }
        
        return (size == 0) ? 0 : (base - arr);
    }

    template<typename T, typename U>
    constexpr size_t upper_bound(const T* arr, size_t size, const U& value)
    {
        T* base = const_cast<T*>(arr);
        size_t len = size;
        
        while (len > 1) 
        {
            size_t half = len / 2;
            base += (!(value < base[half - 1])) * half;
            len -= half;
        }
        
        return (size == 0) ? 0 : (base - arr + (!(value < *base)));
    }

    template<typename T, typename U>
    constexpr bool binary_search(const T* arr, size_t size, const U& value)
    {
        if (size == 0) 
            return false;
        
        size_t pos = lower_bound(arr, size, value);
        return (pos < size && !(value < arr[pos]));
    }

    template<typename T>
    void isort(T* arr, size_t size)
    {
        for (size_t i = 1; i < size; i++)
        {
            T key = move(arr[i]);
            size_t j = i;
            
            while (j > 0 && arr[j-1] > key)
            {
                arr[j] = move(arr[j-1]);
                j--;
            }
            
            arr[j] = move(key);
        }
    }

    template<typename T>
    void sort(T* arr, size_t size) /* TBA */
    {
        if (size <= 1)
            return;
        
        isort(arr, 0, size);
    }
}
