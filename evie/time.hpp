/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_TIME_HPP
#define EVIE_TIME_HPP

#include "decimal.hpp"

#include <cstdint>

#include <time.h>

struct ttime_t
{
    //value equals (unix_time * 10^9 + nanoseconds)
    static const i64 exponent = -9;
    static const i64 frac = 1000000000;
    i64 value;
};

inline ttime_t cur_ttime()
{
    timespec t;
	clock_gettime(CLOCK_REALTIME, &t);

    return ttime_t{i64(t.tv_sec) * ttime_t::frac + i64(t.tv_nsec)};
}

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

inline ttime_t cur_ttime_seconds()
{
    return seconds(time(NULL));
}

#endif

