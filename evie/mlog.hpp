/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "myitoa.hpp"
#include "string.hpp"
#include "mtime.hpp"

#include <type_traits>

class simple_log;
static const str_holder mlog_fixed_str[] = {
   str_holder(""), str_holder("0"), str_holder("00"), str_holder("000"),
   str_holder("0000"), str_holder("00000"), str_holder("000000"), str_holder("0000000")
};

template<uint32_t sz>
struct mlog_fixed
{
   mlog_fixed(uint32_t value) : value(value)
   {
      static_assert(sz <= 8, "out of range");
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

#include "time.hpp"

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
    explicit print_binary(const uint8_t* data, uint32_t size) : data(data), size(size)
    {
    }
};

str_holder itoa_hex(uint8_t ch);

template<typename stream>
stream& operator<<(stream& log, const print_binary& v)
{
    const uint8_t *it = &v.data[0], *it_e = &v.data[v.size];
    for(uint32_t i = 0; it != it_e; ++i, ++it) {
        if(i)
            log << ' ';
        log << itoa_hex(*it);
    }
    return log;
}

struct mlog
{
    enum {
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
        only_cout = 1024
    };

    mlog(simple_log* log, long extra_param = info);
    mlog(long extra_param = info);
    ~mlog();
    mlog(const mlog&) = delete;

    mlog& operator<<(char s);
    mlog& operator<<(const str_holder& str)
    {
        write_string(str.str, str.size);
        return *this;
    }
    template<typename array>
    typename std::enable_if_t<std::is_array<array>::value, mlog&> operator<<(const array& v)
    {
        static_assert(sizeof(v[0]) == 1, "char array");
        uint32_t sz = sizeof(v);
        if(v[sz - 1] == char())
            --sz;
        write_string(&v[0], sz);
        return *this;
    }
    mlog& operator<<(const std::string& s);
    template<uint32_t stack_sz> 
    mlog& operator<<(const my_basic_string<char, stack_sz>& s)
    {
        uint32_t sz = s.size();
        write_string(s.begin(), sz);
        return *this;
    }
    mlog& operator<<(const std::exception& e);

    mlog& operator<<(std::ostream& (std::ostream&));
    mlog& operator<<(const mtime_date& d);
    mlog& operator<<(const mtime_time_duration& t);
    mlog& operator<<(const mtime_parsed& p);
    mlog& operator<<(const mtime& p);

    template<typename T>
    typename std::enable_if_t<std::is_integral<T>::value, mlog&> operator<<(T t)
    {
        check_size(22);
        cur_size += my_cvt::itoa(&buf[cur_size], t);
        return *this;
    }
    mlog& operator<<(uint8_t v)
    {
        return (*this) << uint16_t(v);
    }
    mlog& operator<<(double d)
    {
        check_size(30);
        cur_size += my_cvt::dtoa(&buf[cur_size], d);
        return *this;
    }
    static void set_no_cout();

private:
    void write_string(const char* from, uint32_t size);
    simple_log& log;
    void init();
    void check_size(uint32_t delta);
    long extra_param;
    char* buf;
    uint32_t cur_size;
    std::vector<std::pair<char*, uint32_t> >* blocks;
};

template<typename type>
mlog& operator<<(mlog& ml, const std::vector<type>& p)
{
    ml << '{';
    for(auto it = p.begin(), it_e = p.end(), itb = it; it != it_e; ++it) {
        if(it != itb)
            ml << ',';
        ml << *it;
    }
    ml << '}';
    return ml;
}

simple_log* log_init(const char* file_name = 0, long params = 0);
simple_log* log_create(const char* file_name, long params = 0);
void log_destroy(simple_log* log);
void log_set(simple_log* sl);
simple_log* log_get();
long& log_params();

void log_test(size_t thread_count, size_t log_count);

void set_trash_thread();
void set_significant_thread();
void log_start_params(int argc, char** argv);

struct log_raii
{
    simple_log* sl;
    log_raii(const std::string& fname, uint32_t params)
    {
        sl = log_create(fname.c_str(), params);
        log_set(sl);
    }
    ~log_raii()
    {
        log_destroy(sl);
    }
};


