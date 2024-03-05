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

#define MPROFILE(id) static const uint64_t CLINE(counter_id) = profilerinfo::instance().register_counter(id); \
    mprofiler CLINE(profile_me)(CLINE(counter_id));
#define MPROFILE_USER(id, time) static const uint64_t CLINE(counter_id) = profilerinfo::instance().register_counter(id); \
    profilerinfo::instance().add_info(CLINE(counter_id), time);

class profilerinfo : public stack_singleton<profilerinfo>
{
    static const uint32_t max_counters = 100;

    struct info
    {
        uint64_t time, time_max, time_min, count;
        const char* name;

        info();
    };

    info counters[max_counters];
    uint64_t cur_counters;

    void print_impl(const long param);

public:

    bool log_on_exit;

    profilerinfo();
    uint64_t register_counter(const char* id);
    void add_info(uint64_t counter_id, uint64_t time);
    void print();
    void set_print_on_exit(bool print_on_exit);
    ~profilerinfo();
};

struct mprofiler
{
    uint64_t counter_id;
    ttime_t time;

    mprofiler(uint64_t counter_id);
    mprofiler(const mprofiler&) = delete;
    ~mprofiler();
};

#endif

