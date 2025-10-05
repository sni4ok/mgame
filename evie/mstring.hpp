/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_MSTRING_HPP
#define EVIE_MSTRING_HPP

#include "vector.hpp"

struct mstring : mvector<char>
{
    typedef mvector<char> base;

    using base::base;
    using base::operator=;

    template<uint64_t sz>
    mstring(const char (&buf)[sz]) : base(buf, buf + sz - 1)
    {
    }
    template<uint64_t sz>
    mstring& operator=(const char (&buf)[sz])
    {
        clear();
        insert(buf, buf + sz - 1);
        return *this;
    }
    template<uint64_t sz>
    mstring& operator+=(const char (&buf)[sz])
    {
        insert(buf, buf + sz - 1);
        return *this;
    }
    template<uint64_t sz>
    mstring operator+(const char (&buf)[sz]) const
    {
        mstring ret(*this);
        ret.insert(buf, buf + sz - 1);
        return ret;
    }

    char_cit c_str() const;
    mstring& operator+=(str_holder r);
    mstring& operator+=(const mstring& r);
    mstring operator+(str_holder r) const;
    mstring operator+(const mstring& r) const;
    mstring& operator+=(char c);
    mstring operator+(char c) const;
};

template<typename stream>
stream& operator<<(stream& s, const mstring& str)
{
    s.write(str.begin(), str.size());
    return s;
}

template<typename type>
requires(is_numeric_v<type>)
mstring to_string(type value)
{
    char buf[cvt::atoi_size_v<type>];
    uint32_t size = cvt::itoa(buf, value);
    return mstring(buf, buf + size);
}

template<typename type>
requires(!is_same_v<type, mstring>)
type lexical_cast(const mstring& str)
{
    return lexical_cast<type>(str.begin(), str.end());
}

template<typename type>
requires(is_same_v<type, mstring>)
inline const mstring& lexical_cast(const mstring& str)
{
    return str;
}

mstring operator+(str_holder l, str_holder r);

template<typename t>
concept __have_str = is_class_v<t> && requires(t* v)
{
    v->str();
};

template<typename type>
requires(__have_str<type>)
mstring operator+(str_holder l, const type& r)
{
    return l + r.str();
}

#endif

