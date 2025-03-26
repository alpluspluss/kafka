/*
 * static implementation of __cxa_atexit and __cxa_finalize for the Kafka kernel
 * note: may later be re-implemented; need mutex/thread synchronization
 */

#include <stddef.h>
#include <cxxrt/atomic.h>

#define MAX_ATEXIT_HANDLERS 256

static struct
{
    void (*fn)(void*);
    void* arg;
    void* dso_handle;
    int used;
} atexit_handlers[MAX_ATEXIT_HANDLERS];

static atomic_int atexit_handler_count;
static atomic_bool atexit_lock;

static void init_atomics(void) 
{
    static int initialized = 0;
    if (!initialized) 
    {
        atomic_int_init(&atexit_handler_count, 0);
        atomic_bool_init(&atexit_lock, 0);
        initialized = 1;
    }
}

static void acquire_lock(void) 
{
    int expected = 0;
    while (!atomic_bool_compare_exchange(&atexit_lock, &expected, 1, MEMORY_ORDER_SEQ_CST))
        expected = 0;
}

static void release_lock(void) 
{
    atomic_bool_store(&atexit_lock, 0, MEMORY_ORDER_SEQ_CST);
}

int __cxa_atexit(void (*func)(void*), void *arg, void *dso_handle)
{
    init_atomics(); /* init atomics */
    acquire_lock();
    
    int result = 1;
    int count = atomic_int_load(&atexit_handler_count, MEMORY_ORDER_SEQ_CST);
    
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
            atexit_handlers[i].used = 1;
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
        atexit_handlers[count].used = 1;

        atomic_int_store(&atexit_handler_count, count + 1, MEMORY_ORDER_SEQ_CST);
        result = 0; /* success */
    }
    
    release_lock();
    return result; /* error: too many handlers */
}

void __cxa_finalize(void *dso_handle) 
{
    init_atomics();
    acquire_lock();
    
    int count = atomic_int_load(&atexit_handler_count, MEMORY_ORDER_SEQ_CST);
    
    /* call the handlers in LIFO manner */
    for (int i = count - 1; i >= 0; i--) 
    {
        if (atexit_handlers[i].used && 
            (dso_handle == NULL || atexit_handlers[i].dso_handle == dso_handle)) 
        {
            void (*fn)(void*) = atexit_handlers[i].fn;
            void* arg = atexit_handlers[i].arg;
            
            atexit_handlers[i].used = 0;
            release_lock();
            (*fn)(arg); /* call ctor, dtor */
            acquire_lock();
        }
    }
    
    release_lock();
}