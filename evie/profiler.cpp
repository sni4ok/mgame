/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "profiler.hpp"
#include "vector.hpp"
#include "atomic.hpp"
#include "utils.hpp"
#include "mtime.hpp"
#include "mlog.hpp"
#include "sort.hpp"

extern "C"
{
    extern char *strdup (const char *__s)
        noexcept (true) __attribute__ ((__malloc__))
        __attribute__ ((__nonnull__ (1)));
}

profiler* profiler_ptr;

profiler::info::info() : time(), time_max(),
    time_min(limits<ttime_t>::max), count(), name()
{
}

struct print_count
{
    ttime_t value;
};

mlog& operator<<(mlog& m, print_count t)
{
    m << t.value.value;
    return m;
}

void profiler::print(long mlog_params)
{
    u64 ncounters = atomic_load(cur_counters);
    if(!ncounters)
        return;

    info counters_cpy[max_counters];
    copy(counters, counters + ncounters, counters_cpy);
    sort(counters_cpy, counters_cpy + ncounters,
        [](const info& l, const info& r)
        {
            if(l.type == r.type)
                return strcmp(l.name, r.name) < 0;
            else
                return l.type < r.type;
        }
    );

    mlog log(mlog_params);
    log << "profiler: \n";

    for(u64 c = 0; c != ncounters; ++c)
    {
        const info i = counters_cpy[c];
        if(!i.count)
            continue;

        auto f = [&]<typename type>(type, auto av)
        {
            log << _str_holder(i.name) << ": avg: " << av
                << ", min: " << type{i.time_min} << ", max: "
                <<  type{i.time_max}
                << ", all: " << type{i.time} << ", count: "
                << i.count << endl;
        };

        if(i.type == time)
        {
            ttime_t time_av = div_int(i.time, i.count);
            f(print_t(), print_t{time_av});
        }
        else
        {
            i64 av = i.time.value * 100 / i.count;
            if(av % 100)
                f(print_count(), p2{av});
            else
                f(print_count(), av / 100);
        }
    }
}

profiler::profiler() : cur_counters()
{
    profiler_ptr = this;
}

u64 profiler::register_counter(char_cit id, type t)
{
    char_it cid = strdup(id);

    for(;;)
    {
        u64 c = atomic_load(cur_counters);

        if(c == max_counters)
        {
            free(cid);
            throw mexception("profiler::register_counter() overloaded");
        }

        info& i = counters[c];
        if(atomic_compare_exchange<char_cit>(i.name, nullptr, cid))
        {
            i.type = t;
            atomic_add(cur_counters, u64(1));
            return c;
        }
    }
}

void profiler::add(u64 counter_id, ttime_t time)
{
    info& i = counters[counter_id];
    i.time += time;

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

    ++i.count;
}

void profiler::set_instance(profiler* p)
{
    assert(!profiler_ptr || profiler_ptr == p);
    profiler_ptr = p;
}

profiler::~profiler()
{
    profiler_ptr = nullptr;
    for(u64 c = 0; c != cur_counters; ++c)
        free((char_it)counters[c].name);
}

