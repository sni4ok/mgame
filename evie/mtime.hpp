/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "time.hpp"

const ttime_t cur_utc_time_delta = seconds(3 * 3600);

constexpr inline uint32_t day_seconds(ttime_t t)
{
    uint64_t v = t.value / ttime_t::frac;
    return uint32_t(v - v % (3600 * 24));
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
    int32_t days;
};

struct date
{
    uint16_t year;
    uint8_t month, day;

    constexpr bool operator==(date r) const {
        return year == r.year && month == r.month && day == r.day;
    }
    constexpr bool operator!=(date r) const {
        return !(*this == r);
    }
    constexpr bool operator<(date r) const {
        if(year != r.year)
            return year < r.year;
        if(month != r.month)
            return month < r.month;
        return day < r.day;
    }
    constexpr bool operator<=(date r) const {
        if(*this == r)
            return true;
        else
            return *this < r;
    }

    date_duration operator-(date r) const;
    date& operator+=(date_duration d);

    date& operator-=(date_duration d) {
        return *this += date_duration(-d.days);
    }
    date operator+(date_duration d) const {
        date td = *this;
        return td += d;
    }
    date operator-(date_duration d) const {
        date td = *this;
        return td -= d;
    }
};

struct time_duration
{
    uint8_t hours, minutes, seconds;
    uint32_t nanos;

    constexpr bool operator<(time_duration r) const {
        if(hours != r.hours)
            return hours < r.hours;
        if(minutes != r.minutes)
            return minutes < r.minutes;
        if(seconds != r.seconds)
            return seconds < r.seconds;
        return nanos < r.nanos;
    }
    constexpr bool operator!=(time_duration r) const {
        return nanos != r.nanos || seconds != r.seconds || minutes != r.minutes || hours != r.hours;
    }
    constexpr uint32_t total_seconds() const {
        return uint32_t(hours) * 3600 + minutes * 60 + seconds;
    }
    constexpr uint64_t total_ns() const {
        return uint64_t(total_seconds()) * ttime_t::frac + nanos;
    }
};

constexpr inline ttime_t time_from(uint32_t day_seconds, time_duration td)
{
    return {(int64_t(day_seconds) + td.total_seconds()) * ttime_t::frac + td.nanos};
}

struct time_parsed
{
    ::date date;
    time_duration duration;
};

time_parsed parse_time(ttime_t time);
time_duration get_time_duration(ttime_t time);
ttime_t pack_time(time_parsed p);
ttime_t time_from_date(date t);

