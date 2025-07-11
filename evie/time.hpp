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
    static const int64_t exponent = -9;
    static const int64_t frac = 1000000000;
    int64_t value;
};

inline ttime_t cur_ttime()
{
    timespec t;
	clock_gettime(CLOCK_REALTIME, &t);

    return ttime_t{int64_t(t.tv_sec) * ttime_t::frac + int64_t(t.tv_nsec)};
}

inline constexpr ttime_t seconds(int64_t s)
{
    return {s * ttime_t::frac};
}

inline constexpr ttime_t milliseconds(int64_t s)
{
    return {s * (ttime_t::frac / 1000)};
}

inline constexpr ttime_t microseconds(int64_t s)
{
    return {s * 1000};
}

inline constexpr int64_t to_seconds(ttime_t time)
{
    return time.value / ttime_t::frac;
}

inline constexpr int64_t to_ms(ttime_t time)
{
    return time.value / (ttime_t::frac / 1000);
}

inline constexpr int64_t to_us(ttime_t time)
{
    return time.value / 1000;
}

inline ttime_t cur_ttime_seconds()
{
    return seconds(time(NULL));
}

#endif

