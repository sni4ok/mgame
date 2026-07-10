/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "stdint.hpp"

struct ttime_t
{
    static const i64
        exponent = -9,
        frac = 1000000000;

    i64 value;
};

ttime_t cur_ttime();
ttime_t cur_ttime_seconds();

inline constexpr ttime_t hours(i64 s)
{
    return {s * 3600 * ttime_t::frac};
}

inline constexpr ttime_t minutes(i64 s)
{
    return {s * 60 * ttime_t::frac};
}

inline constexpr ttime_t seconds(i64 s)
{
    return {s * ttime_t::frac};
}

inline constexpr ttime_t milliseconds(i64 s)
{
    return {s * (ttime_t::frac / 1000)};
}

inline constexpr ttime_t microseconds(i64 s)
{
    return {s * 1000};
}

inline constexpr i64 to_seconds(ttime_t time)
{
    return time.value / ttime_t::frac;
}

inline constexpr i64 to_ms(ttime_t time)
{
    return time.value / (ttime_t::frac / 1000);
}

inline constexpr i64 to_us(ttime_t time)
{
    return time.value / 1000;
}

