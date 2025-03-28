/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

namespace kfk 
{
    struct Node
    {
        Node* prev = nullptr;
        Node* next = nullptr;

        void unlink()
        {
            if (prev) 
                prev->next = next;
            if (next) 
                next->prev = prev;
            prev = next = nullptr;
        }
    };

    template<typename T, Node T::*NodePtr>
    class List
    {
    public:
        static T* container_of(Node* node)
        {
            return reinterpret_cast<T*>(
                reinterpret_cast<char*>(node) - 
                reinterpret_cast<char*>(&(reinterpret_cast<T*>(0)->*NodePtr))
            );
        }

        List()
        {
            head.next = &head;
            head.prev = &head;
        }

        void push_back(T* item)
        {
            Node* node = &(item->*NodePtr);
            Node* last = head.prev;

            node->next = &head;
            node->prev = last;
            last->next = node;
            head.prev = node;
        }

        void push_front(T* item)
        {
            Node* node = &(item->*NodePtr);
            Node* first = head.next;
            
            node->next = first;
            node->prev = &head;
            first->prev = node;
            head.next = node;
        }

        void remove(T* item)
        {
            Node* node = &(item->*NodePtr);
            node->unlink();
        }
        
        [[nodiscard]] bool empty() const noexcept
        {
            return head.next == &head;
        }

        class Iterator
        {
        public:
            explicit Iterator(Node* n) : node(n) {}
            
            T* operator*() const
            {
                return List::container_of(node);
            }
            
            Iterator& operator++()
            {
                node = node->next;
                return *this;
            }
            
            bool operator!=(const Iterator& other) const
            {
                return node != other.node;
            }
        private:
            Node* node;
        };

        Iterator begin()
        {
            return Iterator(head.next);
        }

        Iterator end()
        {
            return Iterator(&head);
        }

    private:
        Node head; /* sentinel node */
    };
}
