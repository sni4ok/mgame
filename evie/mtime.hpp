/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

const int32_t cur_utc_time = 3;

struct mtime
{
#ifdef WIN32
    struct timespec{
        time_t tv_sec;
        long tv_nsec;
    };
#endif
    timespec t;
    mtime() : t(){
    }
    mtime(uint32_t total_seconds, uint32_t ns) : t(){
        t.tv_sec = total_seconds;
        t.tv_nsec = ns;
    }
    uint32_t total_sec() const{
        return t.tv_sec;
    }
    uint32_t day_seconds() const{
        uint32_t frac = t.tv_sec % (24 * 3600);
        return t.tv_sec - frac;
    }
    uint32_t ms() const{
        return t.tv_nsec / 1000000;
    }
    uint64_t total_ms() const{
        return uint64_t(t.tv_sec) * 1000 + ms();
    }
    uint32_t micros() const{
        return t.tv_nsec / 1000;
    }
    uint32_t nanos() const{
        return t.tv_nsec;
    }
    bool operator<(const mtime& r) const{
        if(t.tv_sec != r.t.tv_sec)
            return t.tv_sec < r.t.tv_sec;
        return t.tv_nsec < r.t.tv_nsec;
    }
    bool operator==(const mtime& r) const{
        return t.tv_sec == r.t.tv_sec && t.tv_nsec == r.t.tv_nsec;
    }
};

inline int64_t get_mtime_delta(const mtime& l, const mtime& r){
    int64_t delta_sec = int64_t(l.total_sec()) - int64_t(r.total_sec());
    int64_t delta_nanos = int64_t(l.nanos()) - int64_t(r.nanos());
    return delta_sec * 1000 * 1000 * 1000 + delta_nanos;
}

inline mtime get_cur_utc_mtime(){
    mtime ret;
#ifndef WIN32
    clock_gettime(CLOCK_REALTIME, &ret.t);
#else
    //TODO: rewrite me
    ret.t.tv_sec = time(NULL);
#endif
    return ret;
}
inline mtime get_cur_utc_mtime_seconds(){
    mtime ret;
    ret.t.tv_sec = time(NULL);
    return ret;
}
inline mtime get_cur_mtime(){
    mtime ret = get_cur_utc_mtime();
    ret.t.tv_sec += cur_utc_time * 3600;
    return ret;
}
inline mtime get_cur_mtime_seconds(){
    mtime ret;
    ret.t.tv_sec = time(NULL) + cur_utc_time * 3600;
    return ret;
}
struct mtime_date
{
    uint32_t year, month, day;
    mtime_date() : year(), month(), day(){
    }
    bool operator==(const mtime_date& r) const{
        return year == r.year && month == r.month && day == r.day;
    }
};
struct mtime_time_duration
{
    uint32_t hours, minutes, seconds;
    uint32_t nanos;
    mtime_time_duration() : hours(), minutes(), seconds(), nanos(){
    }
    mtime_time_duration(uint32_t hours, uint32_t minutes, uint32_t seconds, uint32_t nanos = 0)
        : hours(hours), minutes(minutes), seconds(seconds), nanos(nanos){
    }
    bool operator<(const mtime_time_duration& r) const{
        if(hours != r.hours)
            return hours < r.hours;
        if(minutes != r.minutes)
            return minutes < r.minutes;
        if(seconds != r.seconds)
            return seconds < r.seconds;
        return nanos < r.nanos;
    }
};
struct mtime_parsed : mtime_date, mtime_time_duration
{
};
void parse_mtime(const mtime& time, mtime_parsed& ret);
mtime_time_duration get_mtime_time_duration(const mtime& time);

