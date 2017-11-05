/*
   This file contains logger for c++ projects, simplest realisation now
*/

#pragma once

#include "utils.hpp"
#include "time.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <mutex>

static const std::string mlog_fixed_str[] = {
   std::string(""), std::string("0"), std::string("00"), std::string("000"),
   std::string("0000"), std::string("00000"), std::string("000000"), std::string("0000000")
};

template<uint32_t sz>
struct mlog_fixed
{
   mlog_fixed(uint32_t value) : value(value)
   {
      static_assert(sz <= 8, "out of range");
   }
   const std::string& str() const
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
stream& operator<<(stream& log, const mlog_fixed<sz>& v){
    log << v.str() << v.value;
    return log;
}

class mlog
{
    //very slow logger now, but threadsafe
    std::ostream& s;
    const uint32_t params;
    friend void init_log(const std::string& fname, uint32_t params);
    bool need_cout() const;
public:

    static const uint32_t info = 0, warning = 2, error = 4,
        cout = 256; //log in file and in STDOUT
    
	mlog(uint32_t params = info);
    template<typename t>
    mlog& operator<<(const t& v){
        s << v;
        if(need_cout())
            std::cout << v;
        return *this;
    }
    mlog& operator<<(const std::exception& e){
        s << "exception: " << e.what();
        if(need_cout())
            std::cout << "exception: " << e.what();
        return *this;
    }
    ~mlog();
};

struct log_init
{
    log_init(const std::string& fname, uint32_t params = mlog::info);
    ~log_init();
};

struct simple_log;
simple_log* log_get();
void log_set(simple_log* sl);
uint32_t& log_params();

struct print_binary
{
    const uint8_t* data;
    uint32_t size;
    explicit print_binary(const uint8_t* data, uint32_t size) : data(data), size(size){
    }
};
std::string itoa_hex(uint8_t c);
template<typename stream>
stream& operator<<(stream& log, const print_binary& v){
    const uint8_t *it = &v.data[0], *it_e = &v.data[v.size];
    for(uint32_t i = 0; it != it_e; ++i, ++it){
        if(i)
            log << ' ';
        log << itoa_hex(*it);
    }
    return log;
}

template<typename stream>
stream& operator<<(stream& s, const ttime_t& t)
{
    char buf[20];
    time_t tt = t.value / 1000000;
    strftime(buf, 20, "%Y-%m-%d %H:%M:%S", localtime(&tt));
    s << buf << "." << mlog_fixed<6>(t.value % 1000000);
    return s;
}

