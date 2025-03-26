/* This file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#ifdef __cplusplus
extern "C" 
{
#endif

#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

enum 
{
    MEMORY_ORDER_ACQUIRE = __ATOMIC_ACQUIRE,
    MEMORY_ORDER_RELEASE = __ATOMIC_RELEASE,
    MEMORY_ORDER_SEQ_CST = __ATOMIC_SEQ_CST
};

typedef struct 
{
    volatile int value;
} atomic_int;

static inline void atomic_int_init(atomic_int* obj, int value) 
{
    obj->value = value;
}

static inline int atomic_int_load(atomic_int* obj, int order) 
{
#if __has_builtin(__atomic_load_n)
    return __atomic_load_n(&obj->value, order);
#else
    int value = obj->value;
    __sync_synchronize();
    return value;
#endif
}

static inline void atomic_int_store(atomic_int* obj, int value, int order) 
{
#if __has_builtin(__atomic_store_n)
    __atomic_store_n(&obj->value, value, order);
#else
    __sync_synchronize();
    obj->value = value;
    __sync_synchronize();
#endif
}

static inline int atomic_int_exchange(atomic_int* obj, int value, int order) 
{
#if __has_builtin(__atomic_exchange_n)
    return __atomic_exchange_n(&obj->value, value, order);
#else
    int old;
    do 
    {
        old = obj->value;
    } 
    while (!__sync_bool_compare_and_swap(&obj->value, old, value));
    return old;
#endif
}

static inline int atomic_int_compare_exchange(atomic_int* obj, int* expected, int desired, int order) {
#if __has_builtin(__atomic_compare_exchange_n)
    return __atomic_compare_exchange_n(&obj->value, expected, desired, 0, order, order);
#else
    int old = *expected;
    int success = __sync_bool_compare_and_swap(&obj->value, old, desired);
    if (!success) {
        *expected = obj->value;
    }
    return success;
#endif
}

typedef struct 
{
    volatile int value;
} atomic_bool;

static inline void atomic_bool_init(atomic_bool* obj, int value) 
{
    obj->value = value ? 1 : 0;
}

static inline int atomic_bool_load(atomic_bool* obj, int order) 
{
    return atomic_int_load((atomic_int*)obj, order) != 0;
}

static inline void atomic_bool_store(atomic_bool* obj, int value, int order) 
{
    atomic_int_store((atomic_int*)obj, value ? 1 : 0, order);
}

static inline int atomic_bool_exchange(atomic_bool* obj, int value, int order) 
{
    return atomic_int_exchange((atomic_int*)obj, value ? 1 : 0, order) != 0;
}

static inline int atomic_bool_compare_exchange(atomic_bool* obj, int* expected, int desired, int order) 
{
    int exp = *expected ? 1 : 0;
    int result = atomic_int_compare_exchange((atomic_int*)obj, &exp, desired ? 1 : 0, order);
    *expected = exp != 0;
    return result;
}

#ifdef __cplusplus
}
#endif