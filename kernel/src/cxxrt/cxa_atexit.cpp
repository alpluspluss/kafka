/* this file is a part of Kafka kernel which is under MIT license; see LICENSE
 * for more info */
/*
 * C++ implementation of __cxa_atexit and __cxa_finalize for the Kafka kernel
 * optimized with better memory ordering and spinlock backoff
 */

#include <atomic.hpp>
#include <kafka/hal/cpu.hpp>

#define MAX_ATEXIT_HANDLERS 256

namespace 
{
    struct AtExitHandler 
    {
        void (*fn)(void *);
        void *arg;
        void *dso_handle;
        bool used;
    };

    AtExitHandler atexit_handlers[MAX_ATEXIT_HANDLERS];

    kfk::Atomic atexit_handler_count(0);
    kfk::Atomic atexit_lock(false);

    void acquire_lock() 
    {
        auto expected = false;
        unsigned int spins = 0;

        while (!atexit_lock.compare_exchange(expected, true, 
            kfk::MemoryOrder::ACQUIRE)) 
        {
             expected = false;

            /* performance: exponentially backoff to avoid */
            for (unsigned int i = 0; i < (1U << spins); ++i)
                kfk::cpu::pause();

            /* cap the maximum backoff */
            if (spins < 10)
                spins++;
        }
    }

    void release_lock() 
    {
        atexit_lock.store(false, kfk::MemoryOrder::RELEASE); 
    }
}

extern "C" int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle)
{
    acquire_lock();

    auto result = 1; /* default to */
    const int count = atexit_handler_count.load(kfk::MemoryOrder::ACQUIRE);

    if (count >= MAX_ATEXIT_HANDLERS) 
    {
        release_lock();
        return result;
    }

    /* find an unused slot first */
    for (int i = 0; i < count; i++) 
    {
        if (!atexit_handlers[i].used) 
        {
            /* reuse */
            atexit_handlers[i].fn = func;
            atexit_handlers[i].arg = arg;
            atexit_handlers[i].dso_handle = dso_handle;
            atexit_handlers[i].used = true;
            result = 0; /* success */
            release_lock();
            return result;
        }
    }

    if (count < MAX_ATEXIT_HANDLERS) 
    {
        atexit_handlers[count].fn = func;
        atexit_handlers[count].arg = arg;
        atexit_handlers[count].dso_handle = dso_handle;
        atexit_handlers[count].used = true;

        atexit_handler_count.store(count + 1, kfk::MemoryOrder::RELEASE);
        result = 0; /* success */
    }

    release_lock();
    return result;
 }

extern "C" void __cxa_finalize(const void *dso_handle)
{
    acquire_lock();

    const int count = atexit_handler_count.load(kfk::MemoryOrder::ACQUIRE);
    for (int i = count - 1; i >= 0; i--) /* call the handlers in LIFO manner */
    {
        if (atexit_handlers[i].used &&
            (dso_handle == nullptr ||
            atexit_handlers[i].dso_handle == dso_handle)) 
        {
            /* this *NEEDS* stored locally for thread-safety */
            auto fn = atexit_handlers[i].fn;
            auto arg = atexit_handlers[i].arg;

            /* mark as unused just before release */
            atexit_handlers[i].used = false;

            /* release then call restore again */
            release_lock();
            (*fn)(arg);
            acquire_lock();
        }
    }

    release_lock();
}
