/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_PROFILER_HPP
#define EVIE_PROFILER_HPP

#include "time.hpp"

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define CLINE(a) CONCAT(a, __LINE__)

#define PROFILE(id, type, pf) static const u64 CLINE(counter_id) = \
    profiler_ptr->register_counter(id, profiler::type); \
    pf CLINE(profile_id)(CLINE(counter_id));

#define MPROFILE(id) PROFILE(id, time, mprofiler)
#define RPROFILE(id) PROFILE(id, rdtsc, rprofiler)

#define MPROFILE_TYPE(id, type, value) static const u64 CLINE(counter_id) = \
    profiler_ptr->register_counter(id, profiler:: type); \
    profiler_ptr->add(CLINE(counter_id), value);

#define MPROFILE_USER(id, value) MPROFILE_TYPE(id, time, value)
#define MPROFILE_COUNT(id, value) MPROFILE_TYPE(id, count, value)

class profiler
{
    static const u64 max_counters = 100;

    struct info
    {
        ttime_t time, time_max, time_min;
        i64 count;
        const char* name;
        int type;

        info();
    };

    info counters[max_counters];
    u64 cur_counters;

    profiler();
    friend class simple_log;

public:
    enum type
    {
        time, rdtsc, count
    };

    u64 register_counter(const char* name, type t);
    void add(u64 counter_id, ttime_t time);
    void print(long mlog_params);
    static void set_instance(profiler* p);
    ~profiler();
};

extern profiler* profiler_ptr;

template<ttime_t (*get_time)()>
struct profile
{
    u64 counter_id;
    ttime_t time;

    profile(u64 counter_id) : counter_id(counter_id), time(get_time())
    {
    }
    ~profile()
    {
        profiler_ptr->add(counter_id, get_time() - time);
    }
    profile(const profile&) = delete;
};

typedef profile<&cur_ttime> mprofiler;

inline ttime_t rdtsc()
{
    return {i64(__builtin_ia32_rdtsc())};
}

typedef profile<&rdtsc> rprofiler;


#endif

