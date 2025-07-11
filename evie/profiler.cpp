/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "profiler.hpp"
#include "vector.hpp"
#include "atomic.hpp"
#include "mtime.hpp"
#include "mlog.hpp"
#include "sort.hpp"

profilerinfo::info::info() : time(), time_max(), time_min(limits<ttime_t>::max), count(), name()
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

        ttime_t time_av = div_int(i.time, i.count);
        log << _str_holder(i.name) << ": avg: " << print_t(time_av)
            << ", min: " << print_t(i.time_min) << ", max: " <<  print_t(i.time_max)
            << ", all: " << print_t(i.time) << ", count: " << i.count << endl;
    }
}

profilerinfo::profilerinfo() : cur_counters()
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

void profilerinfo::add_info(uint64_t counter_id, ttime_t time)
{
    if(!time) [[unlikely]]
        time = {1};

    info& i = counters[counter_id];
    atomic_add(i.time, time);

    ttime_t t;
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
}

mprofiler::mprofiler(uint64_t counter_id) : counter_id(counter_id), time(cur_ttime())
{
}

mprofiler::~mprofiler()
{
    profilerinfo::instance().add_info(counter_id, cur_ttime() - time);
}

