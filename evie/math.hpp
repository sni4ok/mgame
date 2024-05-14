/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "type_traits.hpp"

extern "C"
{
    double sqrt(double x) noexcept;
    double modf(double x, double *iptr) noexcept;
    double pow(double x, double y) noexcept;
    double exp10(double x) noexcept;
    double log(double x) noexcept;
}

template<typename type>
requires(is_signed_v<type>)
type abs(type v)
{
    if(v < type())
        return -v;
    return v;
}

