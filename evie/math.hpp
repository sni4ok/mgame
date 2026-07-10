/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "stdint.hpp"

extern "C"
{
    double sqrt(double x) noexcept;
    double modf(double x, double *iptr) noexcept;
    double pow(double x, double y) noexcept;
    double exp10(double x) noexcept;
    double log(double x) noexcept;
}

template<typename type>
requires(is_signed<type>::value)
constexpr type abs(type v)
{
    if(v < type())
        return -v;
    return v;
}

template<typename type>
constexpr type pow(type f, u32 e)
{
    if(!e)
        return type(1);
    return f * pow(f, e - 1);
}

template<typename type>
constexpr type min(type l, type r)
{
    if(l < r)
        return l;
    return r;
}

template<typename type>
constexpr type max(type l, type r)
{
    if(l > r)
        return l;
    return r;
}

