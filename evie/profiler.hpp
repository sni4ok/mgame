/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_PROFILER_HPP
#define EVIE_PROFILER_HPP

#include "time.hpp"
#include "singleton.hpp"

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define CLINE(a) CONCAT(a, __LINE__)

#define MPROFILE(id) static const u64 CLINE(counter_id) = \
    profiler::instance().register_counter(id, profiler::time); \
    mprofiler CLINE(profile_id)(CLINE(counter_id));

#define MPROFILE_TYPE(id, type, value) static const u64 CLINE(counter_id) = \
    profiler::instance().register_counter(id, profiler:: type); \
    profiler::instance().add(CLINE(counter_id), value);

#define MPROFILE_USER(id, value) MPROFILE_TYPE(id, time, value)
#define MPROFILE_COUNT(id, value) MPROFILE_TYPE(id, count, value)

class profiler : public stack_singleton<profiler>
{
    static const u32 max_counters = 100;

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
        time, count
    };

    u64 register_counter(const char* name, type t);
    void add(u64 counter_id, ttime_t time);
    void print(long mlog_params);
    ~profiler();
};

struct mprofiler
{
    u64 counter_id;
    ttime_t time;

    mprofiler(u64 counter_id);
    mprofiler(const mprofiler&) = delete;
    ~mprofiler();
};

#endif

