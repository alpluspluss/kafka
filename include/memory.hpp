/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <atomic.hpp>
#include <types.hpp>
#include <type_traits.hpp>
#include <utilities.hpp>
#include <kafka/heap.hpp>

namespace kfk
{
    template<typename T>
    struct DefaultDeleter
    {
        void operator()(T* ptr) const noexcept
        {
            if constexpr(!is_trivial<T>::value)
                ptr->~T();
            heap::free(ptr);
        }
    };

    /* specialized deleter for arrays */
    template<typename T>
    struct DefaultDeleter<T[]>
    {
        void operator()(T* ptr) const noexcept
        {
            if constexpr(!is_trivial<T>::value)
            {
                /* call destructor for each element */
                /* we don't have the array size, so this will need to be tracked separately */
                /* or used with special make_unique_array that stores size metadata */
            }
            heap::free(ptr);
        }
    };

    template<typename T, typename Deleter = DefaultDeleter<T>>
    class UniquePtr
    {
    public:
        constexpr UniquePtr() noexcept : ptr(nullptr), deleter() {}
        
        explicit UniquePtr(T* ptr) noexcept : ptr(ptr), deleter() {}

        UniquePtr(T* ptr, const Deleter& deleter) : ptr(ptr), deleter(deleter) {}

        /* no-copy; move only */
        UniquePtr(const UniquePtr&) = delete;

        UniquePtr& operator=(const UniquePtr&) = delete;

        UniquePtr(UniquePtr&& other) noexcept : ptr(other.ptr), deleter(kfk::move(other.deleter))
        {
            other.ptr = nullptr;
        }

        UniquePtr& operator=(UniquePtr&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                ptr = other.ptr;
                deleter = kfk::move(other.deleter);
                other.ptr = nullptr;
            }
            return *this;
        }

        [[nodiscard]] T& get() const noexcept
        {
            return ptr;
        }

        T& operator*() const noexcept
        {
            return *ptr;
        }

        T* operator->() const noexcept
        {
            return ptr;
        }

        explicit operator bool() const noexcept
        {
            return ptr != nullptr;
        }

        void reset(T* ptr = nullptr) noexcept
        {
            if (this->ptr != nullptr)
                deleter(this->ptr);
            this->ptr = ptr;
        }

        T* release() noexcept
        {
            T* tmp = ptr;
            ptr = nullptr;
            return tmp;
        }

        void swap(UniquePtr& other) noexcept
        {
            kfk::swap(ptr, other.ptr);
            kfk::swap(deleter, other.deleter);
        }

    private:
        T* ptr;
        Deleter deleter;
    };

    template<typename T, typename Deleter>
    class UniquePtr<T[], Deleter>
    {
    public:
        constexpr UniquePtr() noexcept : ptr(nullptr), deleter() {}

        explicit UniquePtr(T* ptr) noexcept : ptr(ptr), deleter() {}

        UniquePtr(const UniquePtr&) = delete;

        UniquePtr& operator=(const UniquePtr&) = delete;

        UniquePtr(UniquePtr&& other) noexcept : ptr(other.ptr), deleter(kfk::move(other.deleter)) 
        {
            other.ptr = nullptr;
        }

        UniquePtr& operator=(UniquePtr&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                ptr = other.ptr;
                deleter = kfk::move(other.deleter);
                other.ptr = nullptr;
            }
            return *this;
        }

        [[nodiscard]] T& get() const noexcept
        {
            return ptr;
        }

        T& operator[](size_t i) const noexcept
        {
            return ptr[i];
        }

        explicit operator bool() const noexcept
        {
            return ptr != nullptr;
        }

        void reset(T* ptr = nullptr) noexcept
        {
            if (this->ptr != nullptr)
                deleter(this->ptr);
            this->ptr = ptr;
        }

        T* release() noexcept
        {
            T* tmp = ptr;
            ptr = nullptr;
            return tmp;
        }

        void swap(UniquePtr& other) noexcept
        {
            kfk::swap(ptr, other.ptr);
            kfk::swap(deleter, other.deleter);
        }

    private:
        T* ptr;
        Deleter deleter;
    };

    template<typename T, typename... Args>
    UniquePtr<T> make_unique(Args&&... args)
    {
        return UniquePtr<T>(new (heap::allocate(sizeof(T))) T(kfk::forward<Args>(args)...));
    }

    template<typename T>
    UniquePtr<T[]> make_unique_array(size_t size)
    {
        using ElementType = typename remove_reference<T>::type;
        ElementType* ptr = static_cast<ElementType*>(heap::allocate(sizeof(ElementType), size));
        
        if (ptr == nullptr)
            return nullptr;
            
        /* init elements if non-trivial */
        if constexpr(!is_trivial<ElementType>::value)
        {
            for (size_t i = 0; i < size; ++i)
                new (&ptr[i]) ElementType();
        }
        
        return UniquePtr<T[]>(ptr);
    }

    template<typename T, typename Deleter>
    class ControlBlock
    {
    public:
        using DestroyFn = void(*)(void*, T*, Deleter&);

        ControlBlock(T* ptr, const Deleter& del, DestroyFn destroy_fn) noexcept
            : ref_count(1), ptr(ptr), deleter(del), destroy(destroy_fn) {}
        
        void increment() noexcept
        {
            ref_count.store(ref_count.load(MemoryOrder::ACQUIRE) + 1, MemoryOrder::RELEASE);
        }
        
        bool decrement() noexcept
        {
            return ref_count.exchange(ref_count.load(MemoryOrder::ACQUIRE) - 1, MemoryOrder::ACQUIRE) == 1;
        }

        Atomic<int> ref_count;
        T* ptr;
        Deleter deleter;
        DestroyFn destroy; /* function pointer to destroy managed object */
    };

    template<typename T>
    class SharedPtr
    {
    public:
        constexpr SharedPtr() noexcept : ptr(nullptr), ctrl(nullptr) {}

        template<typename U, typename Deleter = DefaultDeleter<U>>
        explicit SharedPtr(U* ptr, const Deleter& deleter = Deleter())
            : ptr(ptr), ctrl(nullptr)
        {
            if (ptr)
            {
                static auto destroy_fn = [](void* block, U* ptr, Deleter& deleter) {
                    deleter(ptr);
                    heap::free(block);
                };

                /* allocate the control block */
                void* block = heap::allocate(sizeof(ControlBlock<U, Deleter>));
                if (!block) /* failure!!!! */
                {
                    deleter(ptr);
                    ptr = nullptr;
                    return;
                }

                ctrl = new (block) ControlBlock<U, Deleter>(ptr, deleter, destroy_fn);
            }
        }

        SharedPtr(SharedPtr&& other) noexcept : ptr(other.ptr), ctrl(other.ctrl)
        {
            other.ptr = nullptr;
            other.ctrl = nullptr;
        }

        SharedPtr& operator=(const SharedPtr& other) noexcept
        {
            if (this != &other)
            {
                reset();
                ptr = other.ptr;
                ctrl = other.ctrl;
                if (ctrl)
                    ctrl = get_ctrl()->increment();
            }
            return *this;
        }

        SharedPtr& operator=(SharedPtr&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                ptr = other.ptr;
                ctrl = other.ctrl;
                other.ptr = nullptr;
                other.ctrl = nullptr;
            }
            return *this;
        }

        ~SharedPtr() noexcept
        {
            reset();
        }
        
        [[nodiscard]] T* get() const noexcept 
        { 
            return ptr; 
        }
        
        T& operator*() const noexcept 
        { 
            return *ptr; 
        }

        T* operator->() const noexcept 
        { 
            return ptr;
        }
        
        explicit operator bool() const noexcept
        { 
            return ptr != nullptr; 
        }
        
        void reset() noexcept
        {
            if (ctrl && get_ctrl()->decrement())
            {
                void* block = ctrl;
                get_ctrl()->destroy(block, get_ctrl()->ptr, get_ctrl()->deleter);
            }
            ptr = nullptr;
            ctrl = nullptr;
        }
        
        /* for db/tests */
        long use_count() const noexcept
        {
            return ctrl ? get_ctrl()->ref_count.load(MemoryOrder::ACQUIRE) : 0;
        }
        
        void swap(SharedPtr& other) noexcept
        {
            kfk::swap(ptr, other.ptr);
            kfk::swap(ctrl, other.ctrl);
        }
    
    private:
        T* ptr;
        void* ctrl; /* note: the implementation uses void* to avoid template fuckery */

        template<typename U, typename D>
        [[nodiscard]] ControlBlock<U, D>* get_ctrl() const noexcept
        {
            return static_cast<ControlBlock<U, D>*>(ctrl);
        }
    };

    template<typename T, typename... Args>
    SharedPtr<T> make_shared(Args&&... args)
    {
        T* ptr = nullptr;
        void* mem = heap::allocate(sizeof(T));
        if (mem)
            ptr = new (mem) T(forward<Args>(args)...);
        
        return SharedPtr<T>(ptr);
    }

    template<typename T, typename U>
    bool operator==(const UniquePtr<T>& lhs, const UniquePtr<U>& rhs) noexcept
    {
        return lhs.get() == rhs.get();
    }

    template<typename T, typename U>
    bool operator!=(const UniquePtr<T>& lhs, const UniquePtr<U>& rhs) noexcept
    {
        return lhs.get() != rhs.get();
    }

    template<typename T>
    bool operator==(const UniquePtr<T>& ptr, nullptr_t) noexcept
    {
        return ptr.get() == nullptr;
    }

    template<typename T>
    bool operator==(nullptr_t, const UniquePtr<T>& ptr) noexcept
    {
        return ptr.get() == nullptr;
    }

    template<typename T>
    bool operator!=(const UniquePtr<T>& ptr, nullptr_t) noexcept
    {
        return ptr.get() != nullptr;
    }

    template<typename T>
    bool operator!=(nullptr_t, const UniquePtr<T>& ptr) noexcept
    {
        return ptr.get() != nullptr;
    }

    template<typename T, typename U>
    bool operator==(const SharedPtr<T>& lhs, const SharedPtr<U>& rhs) noexcept
    {
        return lhs.get() == rhs.get();
    }

    template<typename T, typename U>
    bool operator!=(const SharedPtr<T>& lhs, const SharedPtr<U>& rhs) noexcept
    {
        return lhs.get() != rhs.get();
    }

    template<typename T>
    bool operator==(const SharedPtr<T>& ptr, nullptr_t) noexcept
    {
        return ptr.get() == nullptr;
    }

    template<typename T>
    bool operator==(nullptr_t, const SharedPtr<T>& ptr) noexcept
    {
        return ptr.get() == nullptr;
    }

    template<typename T>
    bool operator!=(const SharedPtr<T>& ptr, nullptr_t) noexcept
    {
        return ptr.get() != nullptr;
    }

    template<typename T>
    bool operator!=(nullptr_t, const SharedPtr<T>& ptr) noexcept
    {
        return ptr.get() != nullptr;
    }
}
