/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "time.hpp"

const int64_t cur_utc_time_delta = 3 * 3600 * ttime_t::frac;

inline ttime_t operator+(ttime_t l, int64_t nanos)
{
    return {l.value + nanos};
}

inline ttime_t operator-(ttime_t l, int64_t nanos)
{
    return {l.value - nanos};
}

inline int64_t operator-(ttime_t l, ttime_t r)
{
    return int64_t(l.value) - int64_t(r.value);
}

inline uint32_t day_seconds(ttime_t t)
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

    date_duration(int32_t days) : days(days) {
    }
};

struct date
{
    uint16_t year;
    uint8_t month, day;

    date(uint16_t y, uint8_t m, uint8_t d) : year(y), month(m), day(d) {
    }
    date() : year(), month(), day() {
    }
    bool operator==(const date& r) const {
        return year == r.year && month == r.month && day == r.day;
    }
    bool operator!=(const date& r) const {
        return !(*this == r);
    }
    bool operator<(const date& r) const {
        if(year != r.year)
            return year < r.year;
        if(month != r.month)
            return month < r.month;
        return day < r.day;
    }
    bool operator<=(const date& r) const {
        if(*this == r)
            return true;
        else
            return *this < r;
    }

    date_duration operator-(const date& r) const;
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

    time_duration() : hours(), minutes(), seconds(), nanos() {
    }
    time_duration(uint8_t h, uint8_t m, uint8_t s, uint32_t n = 0)
        : hours(h), minutes(m), seconds(s), nanos(n)
    {
    }
    bool operator<(const time_duration& r) const {
        if(hours != r.hours)
            return hours < r.hours;
        if(minutes != r.minutes)
            return minutes < r.minutes;
        if(seconds != r.seconds)
            return seconds < r.seconds;
        return nanos < r.nanos;
    }
    uint32_t total_seconds() const {
        return uint32_t(hours) * 3600 + minutes * 60 + seconds;
    }
    uint64_t total_ns() const {
        return uint64_t(total_seconds()) * ttime_t::frac + nanos;
    }
};

inline ttime_t time_from(uint32_t day_seconds, const time_duration& td)
{
    return {(uint64_t(day_seconds) + td.total_seconds()) * ttime_t::frac + td.nanos};
}

struct time_parsed : date, time_duration
{
    ::date& date()
    {
        return *this;
    }
    const ::date& date() const
    {
        return *this;
    }
    time_duration& duration()
    {
        return *this;
    }
    const time_duration& duration() const
    {
        return *this;
    }
};

time_parsed parse_time(const ttime_t& time);
time_duration get_time_duration(const ttime_t& time);
ttime_t pack_time(const time_parsed& p);
ttime_t time_from_date(const date& t);

