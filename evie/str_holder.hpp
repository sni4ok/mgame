/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "assert.hpp"
#include "stdlib.hpp"

static const char endl = '\n';

template<typename type>
class span
{
    const type* data;
    u64 size_;

public:
    typedef const type* iterator;

    constexpr span() : data(), size_()
    {
    }
    constexpr span(iterator data, u64 size) : data(data), size_(size)
    {
    }
    constexpr span(iterator from, iterator to) : data(from), size_(to - from)
    {
    }

    template<u64 sz>
    constexpr span(const type (&buf)[sz]) : data(buf), size_(sz - 1)
    {
    }
    constexpr iterator begin() const
    {
        return data;
    }
    constexpr iterator end() const
    {
        return data + size_;
    }
    constexpr auto& operator[](u64 idx) const
    {
        return *(data + idx);
    }
    constexpr auto& back() const
    {
        ASSERT(size_);
        return *(end() - 1);
    }
    constexpr u64 size() const
    {
        return size_;
    }
    constexpr bool empty() const
    {
        return !size_;
    }
    constexpr void resize(u64 size)
    {
        ASSERT(size <= size_);
        size_ = size;
    }
    constexpr void pop_back()
    {
        ASSERT(size_);
        --size_;
    }
};

typedef span<char> str_holder;

str_holder _str_holder(char_cit str);

template<u64 sz>
constexpr str_holder from_array(const char (&str)[sz])
{
    u64 size = sz;
    while(size && str[size - 1] == char())
        --size;
    return {str, str + size};
}

template<typename stream>
stream& operator<<(stream& s, str_holder str)
{
    s.write(str.begin(), str.size());
    return s;
}

bool operator==(str_holder l, str_holder r);

[[noreturn]] void throw_exception(str_holder);
[[noreturn]] void throw_exception(str_holder, str_holder);

