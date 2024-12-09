/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <cstdint>

#ifndef TTIME_T_DEFINED
struct ttime_t
{
    //value equals (unix_time * 10^9 + nanoseconds)
    static const uint64_t frac = 1000000000;
    uint64_t value;
};
#define TTIME_T_DEFINED
#endif

struct price_t
{
    static const int64_t exponent = -5;
    static const int64_t frac = 100000;
    int64_t value;
};

struct count_t
{
    static const int64_t exponent = -8;
    static const int64_t frac = 100000000;
    int64_t value;
};

constexpr inline bool operator<(price_t l, price_t r)
{
    return l.value < r.value;
}

constexpr inline bool operator>(price_t l, price_t r)
{
    return l.value > r.value;
}

constexpr inline bool operator!=(price_t l, price_t r)
{
    return l.value != r.value;
}

constexpr inline bool operator==(price_t l, price_t r)
{
    return l.value == r.value;
}

constexpr inline bool operator<(count_t l, count_t r)
{
    return l.value < r.value;
}

constexpr inline bool operator<=(count_t l, count_t r)
{
    return l.value <= r.value;
}

constexpr inline bool operator>(count_t l, count_t r)
{
    return l.value > r.value;
}

constexpr inline bool operator>=(count_t l, count_t r)
{
    return l.value >= r.value;
}

constexpr inline bool operator!=(count_t l, count_t r)
{
    return l.value != r.value;
}

constexpr inline bool operator==(count_t l, count_t r)
{
    return l.value == r.value;
}

