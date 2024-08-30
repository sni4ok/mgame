/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_MLOG_HPP
#define EVIE_MLOG_HPP

#include "myitoa.hpp"
#include "mtime.hpp"
#include "smart_ptr.hpp"

class simple_log;

static constexpr str_holder mlog_fixed_str[] =
{
   "", "0", "00",
   "000", "0000", "00000",
   "000000", "0000000", "00000000"
};

template<uint32_t sz>
struct mlog_fixed
{
   const uint32_t value;

   mlog_fixed(uint32_t value) : value(value)
   {
      static_assert(sz <= 9, "out of range");
   }
   const constexpr str_holder& str() const
   {
      uint32_t v = value;
      uint32_t idx = sz - 1;
      while(v > 9 && idx)
      {
         v /= 10;
         --idx;
      }
      return mlog_fixed_str[idx];
   }
};

template<typename stream, uint32_t sz>
stream& operator<<(stream& log, const mlog_fixed<sz>& v)
{
    log << v.str() << v.value;
    return log;
}

typedef mlog_fixed<2> print2chars;

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
        node* head;
        node* tail;
        uint32_t extra_param;
    };

private:
    simple_log& log;
    data buf;

    void check_size(uint32_t delta);
    void init();
    mlog(const mlog&) = delete;

public:

    enum
    {
        store_pid = 1,
        store_tid = 2,
        always_cout = 4,
        lock_file = 8,
        no_crit_file = 16,

        info = 32,
        warning = 64,
        error = 128,
        critical = 256,
        no_cout = 512,
    };

    mlog(simple_log* log, uint32_t extra_param = info);
    mlog(uint32_t extra_param = info);
    ~mlog();
    void write(char_cit v, uint32_t s);
    static void set_no_cout();

    template<typename type>
    void write_numeric(type v)
    {
        check_size(my_cvt::atoi_size<type>::value);
        buf.tail->size += my_cvt::itoa(&buf.tail->buf[buf.tail->size], v);
    }
};

void simple_log_free(simple_log* ptr);
unique_ptr<simple_log, simple_log_free> log_init(char_cit file_name = 0, uint32_t params = 0, bool set_log_instance = true);
simple_log* log_get();
void log_set(simple_log* sl);
uint32_t& log_params();
void log_test(size_t thread_count, size_t log_count);

#endif

