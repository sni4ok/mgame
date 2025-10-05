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

#define MPROFILE(id) static const u64 CLINE(counter_id) = profilerinfo::instance().register_counter(id); \
    mprofiler CLINE(profile_me)(CLINE(counter_id));
#define MPROFILE_USER(id, time) static const u64 CLINE(counter_id) = \
    profilerinfo::instance().register_counter(id); \
    profilerinfo::instance().add_info(CLINE(counter_id), time);

class profilerinfo : public stack_singleton<profilerinfo>
{
    static const u32 max_counters = 100;

    struct info
    {
        ttime_t time, time_max, time_min;
        i64 count;
        const char* name;

        info();
    };

    info counters[max_counters];
    u64 cur_counters;

    void print_impl(long mlog_params);
    profilerinfo();

    friend class simple_log;
public:

    u64 register_counter(const char* id);
    void add_info(u64 counter_id, ttime_t time);
    void print(long mlog_params);
    ~profilerinfo();
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

