/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_MLOG_HPP
#define EVIE_MLOG_HPP

#include "myitoa.hpp"
#include "smart_ptr.hpp"
#include "time.hpp"

class simple_log;

struct mlog : ios_base
{
    static const uint32_t buf_size = 200;

    struct node
    {
        node* next;
        uint32_t size;
        char buf[buf_size];
    };

    struct data
    {
        ttime_t time;
        uint32_t pid, tid;
        node head;
        node* tail;
        uint32_t params;
    };

private:
    simple_log& log;
    data* buf;

    void init(uint32_t extra_param);
    mlog(const mlog&) = delete;
    void check_size(uint32_t delta);

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

    mlog(uint32_t extra_param = info);
    mlog(simple_log* log, uint32_t extra_param = info);
    ~mlog();
    void write(char_cit v, uint32_t s);
    static void set_no_cout();

    template<typename type>
    void write_numeric(type v)
    {
        check_size(my_cvt::atoi_size<type>::value);
        buf->tail->size += my_cvt::itoa(&buf->tail->buf[buf->tail->size], v);
    }
};

void free_simple_log(simple_log* ptr);
unique_ptr<simple_log, free_simple_log> log_init(char_cit file_name = nullptr,
    uint32_t params = 0, bool set_log_instance = true);
simple_log* log_get();
void log_set(simple_log* sl);
uint32_t& log_params();
void log_test(size_t thread_count, size_t log_count);

#endif

