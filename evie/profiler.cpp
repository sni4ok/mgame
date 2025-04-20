/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "profiler.hpp"
#include "vector.hpp"
#include "atomic.hpp"
#include "mtime.hpp"
#include "mlog.hpp"
#include "sort.hpp"

struct write_time
{
    uint64_t time;

    explicit write_time(uint64_t time) : time(time)
    {
    }
};

template<typename stream>
static stream& operator<<(stream &log,  const write_time& t)
{
    uint64_t sec = t.time / ttime_t::frac;
    if(sec)
    {
        log << sec << "sec";
        if(sec < 100)
            log << " (" << (t.time / 1000000) << "ms)";
        return log;
    }

    uint64_t ms = t.time / 1000000;
    if(ms)
    {
        log << ms << "ms";
        if(ms < 100)
            log << " (" << (t.time / 1000) << "us)";
        return log;
    }

    log << (t.time / 1000) << "us";
    if(t.time / 1000 < 100)
        log << " (" << t.time << "ns)";
    return log;
}

profilerinfo::info::info() : time(), time_max(), time_min(limits<uint64_t>::max), count(), name()
{
}

void profilerinfo::print_impl(long mlog_params)
{
    uint64_t ncounters = atomic_load(cur_counters);
    if(!ncounters)
        return;

    info counters_cpy[max_counters];
    copy(counters, counters + ncounters, counters_cpy);
    sort(counters_cpy, counters_cpy + ncounters, [](const info& l, const info& r)
        {return strcmp(l.name, r.name) < 0;}
    );

    mlog log(mlog_params);
    log << "profiler: \n";
    for(uint64_t c = 0; c != ncounters; ++c)
    {
        const info i = counters_cpy[c];
        if(!i.count)
            continue;
        uint64_t time_av = i.time / i.count;
        log << _str_holder(i.name) << ": average time: " << write_time(time_av)
            << ", minimum time: " << write_time(i.time_min) << ", maximum time: " <<  write_time(i.time_max)
            << ", all time: " << write_time(i.time) << ", count: " << i.count << endl;
    }
}

profilerinfo::profilerinfo() : cur_counters(), log_on_exit(true)
{
}

uint64_t profilerinfo::register_counter(char_cit id)
{
    for(;;)
    {
        uint64_t c = atomic_load(cur_counters);

        if(c == max_counters)
            throw mexception("profilerinfo::register_counter() overloaded");

        if(atomic_compare_exchange(counters[c].name, nullptr, id))
        {
            atomic_add(cur_counters, 1);
            return c;
        }
    }
}

void profilerinfo::add_info(uint64_t counter_id, uint64_t time)
{
    if(!time)
        time = 1;
    info& i = counters[counter_id];
    atomic_add(i.time, time);

    uint64_t t;
    do
    {
        t = atomic_load(i.time_max);
        if(time <= t)
            break;
    }
    while(!atomic_compare_exchange(i.time_max, t, time));

    do
    {
        t = atomic_load(i.time_min);
        if(time >= t)
            break;
    }
    while(!atomic_compare_exchange(i.time_min, t, time));

    atomic_add(i.count, 1);
}

void profilerinfo::print(long mlog_params)
{
    print_impl(mlog_params);
}

profilerinfo::~profilerinfo()
{
    if(log_on_exit)
        print_impl(mlog::info);
}

mprofiler::mprofiler(uint64_t counter_id) : counter_id(counter_id), time(cur_ttime())
{
}

mprofiler::~mprofiler()
{
    profilerinfo::instance().add_info(counter_id, uint64_t((cur_ttime() - time).value));
}

