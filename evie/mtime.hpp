/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

const int32_t cur_utc_time = 3;

#ifdef WIN32
struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};
#endif

struct mtime : timespec
{
    mtime() : timespec() {
    }
    mtime(uint32_t total_seconds, uint32_t ns) : timespec({total_seconds, ns}) {
    }
    uint32_t total_sec() const {
        return tv_sec;
    }
    uint32_t day_seconds() const {
        uint32_t frac = tv_sec % (24 * 3600);
        return tv_sec - frac;
    }
    uint32_t ms() const {
        return tv_nsec / 1000000;
    }
    uint64_t total_ms() const {
        return uint64_t(tv_sec) * 1000 + ms();
    }
    uint32_t micros() const {
        return tv_nsec / 1000;
    }
    uint32_t nanos() const {
        return tv_nsec;
    }
    bool operator<(const mtime& r) const {
        if(tv_sec != r.tv_sec)
            return tv_sec < r.tv_sec;
        return tv_nsec < r.tv_nsec;
    }
    bool operator==(const mtime& r) const {
        return tv_sec == r.tv_sec && tv_nsec == r.tv_nsec;
    }
};

inline int64_t mtime_delta(const mtime& l, const mtime& r)
{
    int64_t delta_sec = int64_t(l.total_sec()) - int64_t(r.total_sec());
    int64_t delta_nanos = int64_t(l.nanos()) - int64_t(r.nanos());
    return delta_sec * 1000 * 1000 * 1000 + delta_nanos;
}

inline mtime cur_utc_mtime()
{
    mtime ret;
#ifndef WIN32
    clock_gettime(CLOCK_REALTIME, &ret);
#else
    //TODO: rewrite me
    ret.tv_sec = time(NULL);
#endif
    return ret;
}

inline mtime cur_utc_mtime_seconds()
{
    mtime ret;
    ret.tv_sec = time(NULL);
    return ret;
}

inline mtime cur_mtime()
{
    mtime ret = cur_utc_mtime();
    ret.tv_sec += cur_utc_time * 3600;
    return ret;
}

inline mtime cur_mtime_seconds()
{
    mtime ret;
    ret.tv_sec = time(NULL) + cur_utc_time * 3600;
    return ret;
}

struct mtime_date
{
    uint16_t year;
    uint8_t month, day;
    mtime_date() : year(), month(), day(){
    }
    bool operator==(const mtime_date& r) const {
        return year == r.year && month == r.month && day == r.day;
    }
};

struct mtime_duration
{
    uint8_t hours, minutes, seconds;
    uint32_t nanos;
    mtime_duration() : hours(), minutes(), seconds(), nanos() {
    }
    mtime_duration(uint8_t h, uint8_t m, uint8_t s, uint8_t n = 0)
        : hours(h), minutes(m), seconds(s), nanos(n)
    {
    }
    bool operator<(const mtime_duration& r) const {
        if(hours != r.hours)
            return hours < r.hours;
        if(minutes != r.minutes)
            return minutes < r.minutes;
        if(seconds != r.seconds)
            return seconds < r.seconds;
        return nanos < r.nanos;
    }
};

struct mtime_parsed : mtime_date, mtime_duration
{
};

mtime_parsed parse_mtime(const mtime& time);
mtime_duration get_mtime_duration(const mtime& time);

