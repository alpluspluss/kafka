/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

namespace kfk
{
    template<typename T>
    struct remove_reference
    {
        using type = T;
    };

    template<typename T>
    struct remove_reference<T&>
    {
        using type = T;
    };

    template<typename T>
    struct remove_reference<T&&>
    {
        using type = T;
    };

    template<typename T>
    struct is_lvalue_reference
    {
        static constexpr bool value = false;
    };

    template<typename T>
    struct is_lvalue_reference<T&>
    {
        static constexpr bool value = true;
    };
}