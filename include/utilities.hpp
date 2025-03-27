/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

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
}
