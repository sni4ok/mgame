/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "mstring.hpp"

struct rational
{
    int32_t num;
    uint32_t den = 1;

    bool operator<(rational v) const
    {
        return int64_t(num) * v.den < int64_t(v.num) * den;
    }
    rational& operator+=(rational v);
    rational operator+(rational v) const
    {
        rational ret = (*this);
        ret += v;
        return ret;
    }
    rational operator-(rational v) const
    {
        return (*this) + rational(-v.num, v.den);
    }
    rational operator*(rational v) const;
    bool operator!() const
    {
        return !num;
    }
    bool operator==(rational v) const
    {
        return int64_t(num) * v.den == int64_t(v.num) * den;
    }
};

template<typename stream>
stream& operator<<(stream& str, rational v)
{
    str << v.num;
    if(v.den != 1)
        str << "/" << v.den;
    return str;
}

mstring to_string(rational value);
template<> rational lexical_cast<rational>(char_cit from, char_cit to);

