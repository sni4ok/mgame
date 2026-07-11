/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef _INITIALIZER_LIST
#define _INITIALIZER_LIST

namespace std
{
    template<typename type>
    struct initializer_list
    {
        const type* buf;
        size_t _size;

        typedef type value_type;
        typedef const type& reference;
        typedef const type& const_reference;
        typedef size_t size_type;
        typedef const type* iterator;
        typedef const type* const_iterator;

        constexpr initializer_list(const_iterator from, size_type size)
            : buf(from), _size(size)
        {
        }
        constexpr initializer_list() : buf(), _size()
        {
        }
        constexpr size_t size() const
        {
            return _size;
        }
        constexpr const_iterator begin() const
        {
            return buf;
        }
        constexpr const_iterator end() const
        {
            return begin() + size();
        }
    };

    template<typename type>
    constexpr const type* begin(initializer_list<type> c)
    {
        return c.begin();
    }

    template<typename type>
    constexpr const type* end(initializer_list<type> c)
    {
        return c.end();
    }
}

#endif

