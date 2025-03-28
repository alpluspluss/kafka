/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <utilities.hpp>

namespace kfk
{
    template<typename K, typename V>
    class UnorderedMap
    {
    public:
        UnorderedMap(size_t init_capacity = 16)
        {
            capacity = init_capacity;
            entries = new Entry[capacity]();
        }

        ~UnorderedMap()
        {
            delete[] entries;
        }

        void insert(const K& key, const V& value)
        {
            if ((sz + 1) * 100 > capacity * 75)
                resize(capacity * 2);

            size_t index = kfk::hash(key);
            int8_t dist = 0;

            K k = key;
            V v = value;

            while (true) /* Robin Hood insertion */
            {
                if (!entries[index].occupied)
                {
                    entries[index].key = k;
                    entries[index].value = v;
                    entries[index].occupied = true;
                    entries[index].prob_distance = dist;
                    sz++;
                    return;
                }

                if (entries[index].key == k)
                {
                    entries[index].value = v;
                    return;
                }
                
                /* if prob distance is higher then swap it */
                if (entries[index].prob_distance < dist)
                {
                    swap(k, entries[index].key);
                    swap(v, entries[index].value);
                    swap(dist, entries[index].prob_distance);
                }
                
                /* go to the next slot */
                index = (index + 1) % capacity;
                dist++;
            }
        }

        [[nodiscard]] bool contains(const K& key, V* out_value = nullptr) const noexcept
        {
            size_t index = kfk::hash(key);
            int8_t dist = 0;

            while (true)
            {
                if (!entries[index].occupied)
                    return false;

                if (entries[index].prob_distance < dist)
                    return false;

                /* found key */
                if (entries[index].key == key)
                {
                    if (out_value)
                        out_value = entries[index].value;

                    return true;
                }

                index = (index + 1) % capacity;
                dist++;
            }
        }

        bool remove(const K& key) noexcept
        {
            size_t index = kfk::hash(key);
            int8_t dist = 0;

            while (true)
            {
                if (!entries[index].occupied) /* empty slot */
                    return false;

                /* key cannot exist if probe distance is less than current distance */
                if (entries[index].prob_distance < dist)
                    return false;

                if (entries[index].key == key)
                {
                    entries[index].occupied = false;
                    sz--;

                    size_t next = (index + 1) % capacity;
                    while (entries[next].occupied && entries[next].prob_distance > 0)
                    {
                        entries[index] = entries[next];
                        entries[index].prob_distance--;

                        entries[next].occupied = false;

                        /* move on */
                        index = next;
                        next = (next + 1) % capacity;
                    }
                }
                return true;
            }
            index = (index + 1) % capacity;
            dist++;
        }

        [[nodiscard]] size_t size() const noexcept
        {
            return sz;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return sz == 0;
        }

    private:
        struct Entry
        {
            K key;
            V value;
            bool occupied = false;
            int8_t prob_distance = -1;
        };
        
        Entry* entries = nullptr;
        size_t capacity = 0;
        size_t sz = 0;

        void resize(size_t new_capacity)
        {
            UnorderedMap<K, V> new_map;
            new_map.capacity = new_capacity;
            new_map.entries = new Entry[new_capacity]();
            
            for (size_t i = 0; i < capacity; ++i)
            {
                if (entries[i].occupied)
                {
                    new_map.insert(entries[i].key, entries[i].value);
                }
            }
            
            delete[] entries;
            entries = new_map.entries;
            capacity = new_map.capacity;
            new_map.entries = nullptr; /* no double-free */
        }
    };
}
