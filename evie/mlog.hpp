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

static const str_holder mlog_fixed_str[] =
{
   str_holder(""), str_holder("0"), str_holder("00"),
   str_holder("000"), str_holder("0000"), str_holder("00000"),
   str_holder("000000"), str_holder("0000000"), str_holder("00000000")
};

template<uint32_t sz>
struct mlog_fixed
{
   mlog_fixed(uint32_t value) : value(value)
   {
      static_assert(sz <= 9, "out of range");
   }
   const str_holder& str() const
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
   const uint32_t value;
};

template<typename stream, uint32_t sz>
stream& operator<<(stream& log, const mlog_fixed<sz>& v)
{
    log << v.str() << v.value;
    return log;
}

typedef mlog_fixed<2> print2chars;

struct print_binary
{
    const uint8_t* data;
    uint32_t size;

    explicit print_binary(const str_holder& str) : data((const uint8_t*)str.str), size(str.size)
    {
    }
    print_binary(const uint8_t* data, uint32_t size) : data(data), size(size)
    {
    }
};

str_holder itoa_hex(uint8_t ch);

template<typename stream>
stream& operator<<(stream& log, const print_binary& v)
{
    const uint8_t *it = &v.data[0], *ie = &v.data[v.size];
    for(uint32_t i = 0; it != ie; ++i, ++it)
    {
        if(i)
            log << ' ';
        log << itoa_hex(*it);
    }
    return log;
}

static const char endl = '\n';

struct mlog
{
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
    mlog(const mlog&) = delete;
    void write(const char* v, uint32_t s);
    mlog& operator<<(char s);

    template<typename array>
    requires(is_array_v<array>)
    mlog& operator<<(const array& v)
    {
        (*this) << from_array(v);
        return *this;
    }

    mlog& operator<<(const std::exception& e);
    mlog& operator<<(const date& d);
    mlog& operator<<(const time_duration& t);
    mlog& operator<<(const time_parsed& p);
    mlog& operator<<(const ttime_t& p);
    mlog& operator<<(double d);
    static void set_no_cout();

    template<typename type>
    requires(is_integral<type>::value)
    mlog& operator<<(type t)
    {
        check_size(my_cvt::atoi_size<type>::value);
        buf.tail->size += my_cvt::itoa(&buf.tail->buf[buf.tail->size], t);
        return *this;
    }

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
    void write_string(const char* from, uint32_t size);
    void check_size(uint32_t delta);
    void init();

    simple_log& log;
    data buf;
};

void simple_log_free(simple_log* ptr);
unique_ptr<simple_log, simple_log_free> log_init(const char* file_name = 0, uint32_t params = 0, bool set_log_instance = true);
simple_log* log_get();
void log_set(simple_log* sl);
uint32_t& log_params();
void log_test(size_t thread_count, size_t log_count);
void cout_write(str_holder str, bool flush = true);
void cerr_write(str_holder str, bool flush = true);

#endif

