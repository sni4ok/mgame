/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "time.hpp"

const ttime_t cur_utc_time_delta = seconds(3 * 3600);

constexpr inline u32 day_seconds(ttime_t t)
{
    u64 v = t.value / ttime_t::frac;
    return u32(v - v % (3600 * 24));
}

inline ttime_t cur_mtime()
{
    return cur_ttime() + cur_utc_time_delta;
}

inline ttime_t cur_mtime_seconds()
{
    return cur_ttime_seconds() + cur_utc_time_delta;
}

struct date_duration
{
    i32 days;
};

struct date
{
    u16 year;
    u8 month, day;

    constexpr bool operator==(date r) const
    {
        return year == r.year && month == r.month && day == r.day;
    }
    constexpr bool operator<(date r) const
    {
        if(year != r.year)
            return year < r.year;
        if(month != r.month)
            return month < r.month;
        return day < r.day;
    }
    constexpr bool operator<=(date r) const
    {
        if(*this == r)
            return true;
        else
            return *this < r;
    }

    date_duration operator-(date r) const;
    date& operator+=(date_duration d);

    date& operator-=(date_duration d)
    {
        return *this += date_duration(-d.days);
    }
    date operator+(date_duration d) const
    {
        date td = *this;
        return td += d;
    }
    date operator-(date_duration d) const
    {
        date td = *this;
        return td -= d;
    }
};

struct time_duration
{
    u8 hours, minutes, seconds;
    u32 nanos;

    constexpr bool operator<(time_duration r) const
    {
        if(hours != r.hours)
            return hours < r.hours;
        if(minutes != r.minutes)
            return minutes < r.minutes;
        if(seconds != r.seconds)
            return seconds < r.seconds;
        return nanos < r.nanos;
    }
    constexpr bool operator==(time_duration r) const
    {
        return nanos == r.nanos && seconds == r.seconds && minutes == r.minutes && hours == r.hours;
    }
    constexpr u32 total_seconds() const
    {
        return u32(hours) * 3600 + minutes * 60 + seconds;
    }
    constexpr operator ttime_t() const
    {
        return ::seconds(total_seconds()) + ttime_t(nanos);
    }
};

struct time_parsed
{
    ::date date;
    time_duration duration;
};

time_parsed parse_time(ttime_t time);
time_duration get_time_duration(ttime_t time);
ttime_t pack_time(time_parsed p);
ttime_t time_from_date(date t);

