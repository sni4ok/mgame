/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "string.hpp"
#include "algorithm.hpp"

template<typename type, u32 size_>
struct carray
{
    type buf[size_];

    carray() : buf() {
    }
    void init(const type* from)
    {
        copy(from, from + size_, buf);
    }
    constexpr carray(std::initializer_list<type> r)
    {
        assert(r.size() <= size_);
        copy(r.begin(), r.end(), buf);
    }
    constexpr carray(span<type> str)
    {
        assert(str.size() <= size_);
        copy(str.begin(), str.end(), buf);
    }

    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;

    static constexpr u32 size()
    {
        return size_;
    }
    static constexpr bool empty()
    {
        return !size_;
    }
    constexpr const_iterator begin() const
    {
        return buf;
    }
    constexpr iterator begin()
    {
        return buf;
    }
    constexpr const_iterator end() const
    {
        return buf + size_;
    }
    constexpr iterator end()
    {
        return buf + size_;
    }
    constexpr const type& operator[](u32 elem) const
    {
        assert(elem < size_);
        return buf[elem];
    }
    constexpr type& operator[](u32 elem)
    {
        assert(elem < size_);
        return buf[elem];
    }
    constexpr span<type> str() const
    {
        if constexpr(is_same_v<type, char>)
            return from_array(buf);
        else
            return span<type>(begin(), end());
    }
    constexpr carray& operator+=(const carray& r)
    {
        for(u32 i = 0; i != size_; ++i)
            buf[i] += r.buf[i];
        return *this;
    }
    constexpr carray operator+(const carray& r) const
    {
        carray ret;
        for(u32 i = 0; i != size_; ++i)
            ret.buf[i] = buf[i] + r.buf[i];
        return ret;
    }
    constexpr carray& operator-=(const carray& r)
    {
        for(u32 i = 0; i != size_; ++i)
            buf[i] -= r.buf[i];
        return *this;
    }
    constexpr carray operator-(const carray& r) const
    {
        carray ret;
        for(u32 i = 0; i != size_; ++i)
            ret.buf[i] = buf[i] - r.buf[i];
        return ret;
    }
};

template<typename type, u32 capacity_>
class array
{
    type buf[capacity_];
    u32 size_;

public:
    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;
    typedef mvector<type>::reverse_iterator reverse_iterator;

    array() : size_()
    {
    }
    explicit array(u32 size) : size_(size)
    {
        assert(size_ <= capacity_);
    }
    array(const array& r) : size_(r.size_)
    {
        assert(size_ <= capacity_);
        copy(r.begin(), r.end(), buf);
    }
    array(span<type> str) : size_(str.size())
    {
        assert(size_ <= capacity_);
        copy(str.begin(), str.end(), buf);
    }
    array(const type* f, const type* t) : size_(t - f)
    {
        assert(size_ <= capacity_);
        copy(f, t, buf);
    }
    template<u32 sz>
    array(const type (&str)[sz]) : size_(sz - 1)
    {
        static_assert(sz - 1 <= capacity_);
        copy(str, str + size_, buf);
    }
    array(std::initializer_list<value_type> r) : size_(r.size())
    {
        assert(size_ <= capacity_);
        copy(r.begin(), r.end(), buf);
    }
    array& operator=(const array& r)
    {
        size_ = r.size_;
        copy(r.begin(), r.end(), buf);
        return *this;
    }
    u32 size() const
    {
        return size_;
    }
    bool empty() const
    {
        return !size_;
    }
    static constexpr u32 capacity()
    {
        return capacity_;
    }
    void clear()
    {
        resize(0);
    }
    void resize(u32 new_size)
    {
        if(new_size < size_)
            size_ = new_size;
        else if(new_size <= capacity_) {
            //for(u32 i = size_; i != new_size; ++i)
            //    buf[i] = type();
            size_ = new_size;
        }
        else
            throw mexception(es() % "array resize for " % new_size % ", capacity " % capacity_);
    }
    void push_back(type&& v)
    {
        assert(size_ < capacity_);
        buf[size_] = move(v);
        ++size_;
    }
    void push_back(const type& v)
    {
        assert(size_ < capacity_);
        buf[size_] = v;
        ++size_;
    }
    const_iterator begin() const
    {
        return buf;
    }
    iterator begin()
    {
        return buf;
    }
    const_iterator end() const
    {
        return buf + size_;
    }
    iterator end()
    {
        return buf + size_;
    }
    reverse_iterator rbegin() const
    {
        return {end() - 1};
    }
    reverse_iterator rend() const
    {
        return {begin() - 1};
    }
    const type& operator[](u32 elem) const
    {
        assert(elem < size_);
        return buf[elem];
    }
    type& operator[](u32 elem)
    {
        assert(elem < size_);
        return buf[elem];
    }
    type& back()
    {
        return buf[size_ - 1];
    }
    const type& back() const
    {
        return buf[size_ - 1];
    }
    void pop_back()
    {
        assert(size_);
        --size_;
    }
    span<type> str() const
    {
        return span<type>(begin(), end());
    }
    void insert(iterator it, const type* from, const type* to)
    {
        u32 size = size_;
        resize(size_ + (to - from));
        copy_backward(it, buf + size, it + (to - from));
        copy(from, to, it);
    }
    iterator insert(iterator it, const type& v)
    {
        insert(it, &v, &v + 1);
        return it;
    }
    iterator erase(iterator from, iterator to)
    {
        copy_backward(to, end(), from);
        size_ -= to - from;
        return from;
    }
    iterator erase(iterator it)
    {
        return erase(it, it + 1);
    }
    void reserve(u32 sz)
    {
        if(sz > capacity_)
            throw str_exception("array::reserve() sz > capacity");
    }
};

template<u32 stack_sz = 252>
using basic_string = array<char, stack_sz>;

typedef basic_string<32> my_trivial_string;
typedef basic_string<> fstring;

template<typename stream, u32 stack_sz>
stream& operator<<(stream& str, const basic_string<stack_sz>& v)
{
    str.write(v.begin(), v.size());
    return str;
}

template<typename type, u32 size>
constexpr auto fill_carray(type v = type())
{
    carray<type, size> ret;
    fill(ret.begin(), ret.end(), v);
    return ret;
}

template<u32 sz>
constexpr auto make_carray(const char (&str)[sz])
{
    return carray<char, sz - 1>(str_holder(str));
}

