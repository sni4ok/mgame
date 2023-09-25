/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"
#include "str_holder.hpp"

struct mstring : mvector<char>
{
    typedef mvector<char> base;
    
    typedef char* iterator;
    typedef const char* const_iterator;
    typedef char value_type;

    mstring()
    {
        __clear();
    }
    explicit mstring(const char* str) : base(str, str + strlen(str))
    {
    }
    mstring(str_holder str) : base(str.str, str.str + str.size)
    {
    }
    mstring(const char* from, const char* to) : base(from, to)
    {
    }
    mstring(const mstring& r) : base(static_cast<const base&>(r))
    {
    }
    mstring(mstring&& r) : base(std::move(static_cast<base&&>(r)))
    {
    }
    mstring& operator=(mstring&& r)
    {
        base::swap(r);
        return *this;
    }
    mstring& operator=(const mstring& r)
    {
        static_cast<base&>(*this) = r;
        return *this;
    }
    void swap(mstring& r)
    {
        base::swap(r);
    }
    str_holder str() const
    {
        return {begin(), size()};
    }
    const char* c_str() const
    {
        mstring* p = const_cast<mstring*>(this);
        p->push_back(char());
        p->resize(p->size() - 1);
        return begin();
    }
    bool operator==(const mstring& r) const
    {
        if(size() == r.size())
            return !memcmp(begin(), r.begin(), size());
        return false;
    }
    bool operator==(const char* str) const
    {
        uint64_t sz = strlen(str);
        if(sz == size())
            return !memcmp(begin(), str, sz);
        return false;
    }
    bool operator!=(const mstring& r) const
    {
        return !(*this == r);
    }
    bool operator<(const mstring& r) const
    {
        return lexicographical_compare(begin(), begin() + size(), r.begin(), r.begin() + r.size());
    }
    mstring& operator+=(const mstring& r)
    {
        base::insert(r.begin(), r.end());
        return *this;
    }
    mstring& operator+=(str_holder r)
    {
        base::insert(r.begin(), r.end());
        return *this;
    }
    mstring operator+(const mstring& r) const
    {
        mstring ret(*this);
        return ret += r;
    }
    mstring operator+(str_holder r) const
    {
        mstring ret(*this);
        return ret += r;
    }
    mstring& operator+=(char c)
    {
        push_back(c);
        return *this;
    }
    mstring operator+(char c) const
    {
        mstring ret(*this);
        return ret += c;
    }
    mstring& operator+=(const char* str)
    {
        base::insert(str, str + strlen(str));
        return *this;
    }
    mstring operator+(const char* str) const
    {
        mstring ret(*this);
        return ret += str;
    }
};

template<typename stream>
stream& operator<<(stream& m, const mstring& s)
{
    m << s.str();
    return m;
}

template<typename type>
std::enable_if_t<std::is_integral<type>::value, mstring>
to_string(type value)
{
    char buf[24];
    uint32_t size = my_cvt::itoa(buf, value);
    return mstring(buf, buf + size);
}

mstring to_string(double value);
template<> mstring lexical_cast<mstring>(char_cit from, char_cit to);

template<typename type>
type lexical_cast(const mstring& str)
{
    return lexical_cast<type>(str.begin(), str.end());
}

mstring operator+(str_holder l, const mstring& r);

