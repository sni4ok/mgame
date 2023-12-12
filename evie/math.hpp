/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <type_traits>

extern "C"
{
    double sqrt(double x) noexcept;
    double modf(double x, double *iptr) noexcept;
    double pow(double x, double y) noexcept;
    double log(double x) noexcept;
}

template<typename type>
requires(std::is_signed<type>::value)
type abs(type v)
{
    if(v < type())
        return -v;
    return v;
}

