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

    mstring();
    explicit mstring(const char* str);
    mstring(str_holder str);
	mstring(std::initializer_list<char> r);
    mstring(const char* from, const char* to);
    mstring(const mstring& r);
    mstring(mstring&& r);
    mstring& operator=(mstring&& r);
    mstring& operator=(const mstring& r);
    void swap(mstring& r);
    str_holder str() const;
    const char* c_str() const;
    bool operator==(const mstring& r) const;
    bool operator==(const char* str) const;
    bool operator!=(const mstring& r) const;
    bool operator<(const mstring& r) const;
    mstring& operator+=(const mstring& r);
    mstring& operator+=(str_holder r);
    mstring operator+(const mstring& r) const;
    mstring operator+(str_holder r) const;
    mstring& operator+=(char c);
    mstring operator+(char c) const;
    mstring& operator+=(const char* str);
    mstring operator+(const char* str) const;
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

