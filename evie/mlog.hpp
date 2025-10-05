/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_MLOG_HPP
#define EVIE_MLOG_HPP

#include "cvt.hpp"
#include "smart_ptr.hpp"
#include "time.hpp"

class simple_log;

struct mlog : ios_base
{
    static const u32 buf_size = 200;

    struct node
    {
        node* next;
        u32 size;
        char buf[buf_size];
    };

    struct data
    {
        ttime_t time;
        u32 pid, tid;
        node head;
        node* tail;
        u32 params;
    };

private:
    simple_log& log;
    data* buf;

    void init(u32 extra_param);
    mlog(const mlog&) = delete;
    void check_size(u32 delta);

public:

    enum
    {
        store_pid = 1,
        store_tid = 2,
        always_cout = 4,
        lock_file = 8,
        no_crit_file = 16,
        truncate_file = 32,
        truncate_crit_file = 64,
        no_profiler = 8192,

        info = 128,
        warning = 256,
        error = 512,
        critical = 1024,
        no_cout = 2048,
        only_cout = 4096
    };

    mlog(u32 extra_param = info);
    mlog(simple_log* log, u32 extra_param = info);
    ~mlog();
    void write(char_cit v, u32 s);
    static void set_no_cout();

    template<typename type>
    void write_numeric(type v)
    {
        check_size(cvt::atoi_size<type>::value);
        buf->tail->size += cvt::itoa(&buf->tail->buf[buf->tail->size], v);
    }
};

void free_simple_log(simple_log* ptr);
unique_ptr<simple_log, free_simple_log> log_init(char_cit file_name = nullptr,
    u32 params = 0, bool set_log_instance = true);
simple_log* log_get();
void log_set(simple_log* sl);
u32& log_params();
void log_test(size_t thread_count, size_t log_count);

struct print_t
{
    ttime_t value;
};

mlog& operator<<(mlog& m, print_t t);

#endif

