/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "mlog.hpp"

#include <cstdint>

#include <time.h>

#ifndef TTIME_T_DEFINED
struct ttime_t
{
    //value equals (unix_time * 10^6 + microseconds)
    uint64_t value;
};
#define TTIME_T_DEFINED
#endif

inline ttime_t get_cur_ttime()
{
    timespec t;
	clock_gettime(CLOCK_REALTIME, &t);

    return ttime_t{uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000};
}

inline bool operator>(ttime_t l, ttime_t r)
{
    return l.value > r.value;
}

inline bool operator<(ttime_t l, ttime_t r)
{
    return l.value < r.value;
}

template<typename stream>
stream& operator<<(stream& s, const ttime_t& t)
{
    char buf[20];
    time_t tt = t.value / 1000000;
    strftime(buf, 20, "%Y-%m-%d %H:%M:%S", localtime(&tt));
    s << buf << "." << mlog_fixed<6>(t.value % 1000000);
    return s;
}

