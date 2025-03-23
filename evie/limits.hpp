/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "decimal.hpp"

#include <float.h>

template<typename t>
struct limits
{
    static constexpr t min = is_signed_v<t> ? t(1) << (sizeof(t) * 8 - 1) : t();
    static constexpr t max = ~min;
};

template<>
struct limits<float>
{
};

template<>
struct limits<double>
{
    static constexpr double min = DBL_MIN;
    static constexpr double max = DBL_MAX;
    static constexpr double infinity = __builtin_huge_val();
    static constexpr double epsilon = DBL_EPSILON;
    static constexpr double quiet_NaN = __builtin_nanl("");
    static constexpr double signaling_NaN = __builtin_nansl("");
};

template<typename decimal>
requires(is_decimal<decimal>)
struct limits<decimal>
{
    static constexpr decimal min = {limits<decltype(decimal::value)>::min};
    static constexpr decimal max = {limits<decltype(decimal::value)>::max};
};

//#define CHECK_LIMITS

template<typename t>
void check_limit()
{
#ifdef CHECK_LIMITS
    limits<t> v;
    std::numeric_limits<t> nl;
    assert(v.min == nl.min());
    assert(v.max == nl.max());
#endif
}

inline void check_limits()
{
    check_limit<short>();
    check_limit<int>();
    check_limit<unsigned>();
    check_limit<long long>();
    check_limit<long unsigned long>();
}

