/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "decimal.hpp"
#include "pair.hpp"

#include <float.h>

template<typename t>
struct __limits
{
    static constexpr t
        min = is_signed_v<t> ? t(-1) << (sizeof(t) * 8 - 1) : t(),
        max = ~min;
};

template<>
struct __limits<float>
{
};

template<>
struct __limits<double>
{
    static constexpr double
        min = DBL_MIN,
        max = DBL_MAX,
        infinity = __builtin_huge_val(),
        epsilon = DBL_EPSILON,
        quiet_NaN = __builtin_nanl(""),
        signaling_NaN = __builtin_nansl("");
};

template<typename decimal>
requires(is_decimal<decimal>)
struct __limits<decimal>
{
    static constexpr decimal
        min = {__limits<decltype(decimal::value)>::min},
        max = {__limits<decltype(decimal::value)>::max};
};

template<typename type>
struct limits : __limits<type>
{
    static constexpr pair<type, type> min_max = {__limits<type>::min, __limits<type>::max};
};

