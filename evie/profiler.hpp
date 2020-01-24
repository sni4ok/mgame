/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "mlog.hpp"
#include "mutex.hpp"
#include "fmap.hpp"
#include "utils.hpp"

#include <cassert>

#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <unistd.h>

#ifndef MPROFILE
#define MPROFILE(id) mprofiler profile_me ## __LINE__ (id, false);
#define MPROFILE_THREAD(id) mprofiler profile_me_thread(id, true);
#endif

static_assert(RUSAGE_THREAD, "rusage_thread");

class profilerinfo : public stack_singleton<profilerinfo>
{
    struct info
    {
        uint64_t time;
        uint64_t time_max, time_min;
        uint64_t count;
    };

    typedef fmap<const char*, info> data_type;
    data_type data;
    my_mutex mutex;

    template<typename type>
    static void write_time(type &t, uint64_t time)
    {
        t << (time / 10000) << "ms";
        if(time / 10000 < 100)
            t << " (" << (time / 10) << "mics)";
    }
    static void print_impl(const data_type& data, const long param)
    {
        if(data.empty())
            return;
        mlog log(param);
        log << "profiler: " << std::endl;
        for(auto& v: data) {
            const info &i = v.second;
            uint64_t time_av = i.time / i.count;
            log << _str_holder(v.first) << ": average time: ";
            write_time(log, time_av);
            log << ", minimum time: ";
            write_time(log, i.time_min);
            log << ", maximum time: ";
            write_time(log, i.time_max);
            log << ", all time: ";
            {
                log << (i.time / 10000000) << "sec";
                if(i.time / 10000000 < 100)
                    log << " (" << (i.time / 10000) << "msec)";
            }
            log << ", count: " << i.count << std::endl;
        }
    }
public:
    profilerinfo()
    {
    }
    void add_info(const char* id, uint64_t time)
    {
        if(!time)
            time = 1;
        my_mutex::scoped_lock lock(mutex);
        info& i = data[id];
        i.time += time;
        i.time_max = std::max(i.time_max, time);
        if(!i.time_min)
            i.time_min = time;
        else
            i.time_min = std::min(i.time_min, time);
        ++i.count;
    }
    void print(bool clear = false)
    {
        data_type data_cpy;
        {
            my_mutex::scoped_lock lock(mutex);
            if(clear)
                data_cpy.swap(data);
            else
                data_cpy = data;
        }
        if(clear)
            mlog(mlog::critical) << "profiler successfully cleared";
        print_impl(data_cpy, mlog::critical);
    }
    void clear()
    {
        data_type tmp;
        {
            my_mutex::scoped_lock lock(mutex);
            tmp.swap(data);
        }
        mlog(mlog::critical) << "profiler successfully cleared";
        print_impl(tmp, mlog::critical);
    }
    ~profilerinfo()
    {
        my_mutex::scoped_lock lock(mutex);
        print_impl(data, mlog::info);
    }
};

class mprofiler_impl
{
    bool cur_thread;
    const char* id;
    uint64_t time; //in 100ns
    mprofiler_impl(const mprofiler_impl&) = delete;

public:
    static void get_process_time(uint64_t& d_time) {
        timeval val = timeval();
        gettimeofday(&val, 0);
        d_time = (uint64_t(val.tv_sec) * 1000000 + uint64_t(val.tv_usec)) * 10;
    }
    static void get_thread_time(uint64_t& r_time) {
        uint64_t user = 0, sys = 0;
        rusage rs = rusage();
        getrusage(RUSAGE_THREAD, &rs);
        user = uint64_t(rs.ru_utime.tv_sec) * 1000000 + uint64_t(rs.ru_utime.tv_usec);
        sys = uint64_t(rs.ru_stime.tv_sec) * 1000000 + uint64_t(rs.ru_stime.tv_usec);
        r_time = (user + sys) * 10;
    }
    mprofiler_impl() {
    }
    void start(const char* id, bool cur_thread){
        this->cur_thread = cur_thread;
        this->id = id;
        time = 0;
        cur_thread ? get_thread_time(time) : get_process_time(time);
    }
    void stop() {
        if(time) {
            uint64_t time_to;
            cur_thread ? get_thread_time(time_to) : get_process_time(time_to);
            if(time_to){
                uint64_t md = time_to > time ? time_to - time : 0;
                profilerinfo::instance().add_info(id, md);
            }
        }
    }
};

struct mprofiler : mprofiler_impl
{
    mprofiler(const char* id, bool cur_thread) {
        start(id, cur_thread);
    }
    ~mprofiler() {
        try {
            stop();
        }
        catch(...) {
        }
    }
};

