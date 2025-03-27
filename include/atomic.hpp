/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

#ifndef __has_feature
#	define __has_feature(x) 0
#endif
#ifndef __has_extension
#	define __has_extension(x) 0
#endif

#if !__has_extension(c_atomic)
#	define _Atomic(T) T
#endif
#if __has_builtin(__c11_atomic_exchange)
#	define ATOMIC_BUILTIN(name) __c11_atomic_##name
#else
#	define ATOMIC_BUILTIN(name) __atomic_##name##_n
#endif

namespace kfk
{
    /* C++11 memory orders; we need only a subset of them */
    enum class MemoryOrder 
    {
        ACQUIRE = __ATOMIC_ACQUIRE, /* acquire order */
        RELEASE = __ATOMIC_RELEASE, /* release order */
        SEQCST = __ATOMIC_SEQ_CST /* sequentially consistent memory ordering; also the default if unspecified  */
    };

    /* the atomic class; a subset of `std::atomic` */
    template<typename T>
    class Atomic
    {
    public:
        constexpr Atomic(T init) : value(init) {}

        /* atomically load with the specified memory order */
        T load(MemoryOrder order = MemoryOrder::SEQCST)
        {
            return ATOMIC_BUILTIN(load)(&value, static_cast<int>(order));
        }

        void store(T v, MemoryOrder order = MemoryOrder::SEQCST)
        {
            return ATOMIC_BUILTIN(store)(&value, v, static_cast<int>(order));
        }

        T exchange(T v, MemoryOrder order = MemoryOrder::SEQCST)
        {
            return ATOMIC_BUILTIN(exchange)(&value, v, static_cast<int>(order));
        }

        bool compare_exchange(T& expected, T desired, MemoryOrder order = MemoryOrder::SEQCST)
        {
#if __has_builtin(__c11_atomic_compare_exchange_strong)
            return __c11_atomic_compare_exchange_strong(&value, &expected, desired, static_cast<int>(order), static_cast<int>(order));
#else
            return __atomic_compare_exchange_n(&value, &expected, desired, true, static_cast<int>(order), static_cast<int>(order));
#endif
        }

    private:
        _Atomic(T) value;

    };
}
