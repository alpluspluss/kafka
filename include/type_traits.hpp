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

    template<typename T, T v>
    struct integral_constant
    {
        static constexpr T value = v;
        using value_type = T;
        using type = integral_constant<T, v>;
        
        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };

    using true_type = integral_constant<bool, true>;
    using false_type = integral_constant<bool, false>;

    template<typename T>
    struct is_trivial : false_type {};

    /* specializations for built-in types that are always trivial */
    template<> struct is_trivial<bool> : true_type {};
    template<> struct is_trivial<char> : true_type {};
    template<> struct is_trivial<signed char> : true_type {};
    template<> struct is_trivial<unsigned char> : true_type {};
    template<> struct is_trivial<short> : true_type {};
    template<> struct is_trivial<unsigned short> : true_type {};
    template<> struct is_trivial<int> : true_type {};
    template<> struct is_trivial<unsigned int> : true_type {};
    template<> struct is_trivial<long> : true_type {};
    template<> struct is_trivial<unsigned long> : true_type {};
    template<> struct is_trivial<long long> : true_type {};
    template<> struct is_trivial<unsigned long long> : true_type {};
    template<> struct is_trivial<float> : true_type {};
    template<> struct is_trivial<double> : true_type {};
    template<> struct is_trivial<long double> : true_type {};
    template<typename T> struct is_trivial<T*> : true_type {};

    /* cv-qualified types have same triviality as base type */
    template<typename T> struct is_trivial<const T> : is_trivial<T> {};
    template<typename T> struct is_trivial<volatile T> : is_trivial<T> {};
    template<typename T> struct is_trivial<const volatile T> : is_trivial<T> {};
}
