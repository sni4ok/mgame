/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <cassert>

template<typename type, uint32_t alloc_size>
struct st_allocator
{
    typedef type value_type;

    template <typename node>
    struct rebind
    {
        typedef st_allocator<node, alloc_size> other;
    };

    type alloc[alloc_size];
    type* links[alloc_size];
    uint32_t size;

    st_allocator() : size()
    {
        for(uint32_t i = 0; i != alloc_size; ++i)
            links[i] = &alloc[i];
    }
    type* allocate(size_t sz)
    {
        if(sz != 1 || size >= alloc_size)
            throw std::bad_alloc();
        return links[size++];
    }
    void deallocate(type* ptr, size_t sz)
    {
        (void)sz;
        assert(sz == 1 && size > 0);
        links[--size] = ptr;
    }
    bool operator!=(const st_allocator<type, alloc_size>&) const
    {
        return true;
    }
    bool operator==(const st_allocator<type, alloc_size>&) const
    {
        return false;
    }
};

