/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <atomic.hpp>
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
}