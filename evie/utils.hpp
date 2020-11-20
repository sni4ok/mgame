/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "time.hpp"
#include "string.hpp"

#include <cassert>

#include <errno.h>

struct noncopyable
{
    noncopyable() {
    }
private:
    noncopyable(const noncopyable&) = delete;
};

template<typename base>
class stack_singleton
{
    static base*& get_impl() {
        static base* value = 0;
        return value;
    }
    stack_singleton(const stack_singleton&) = delete;
public:
    static void set_instance(base* instance) {
        assert((!get_impl() || get_impl() == instance) && "singleton already initialized");
        get_impl() = instance;
    }
    static base& instance() {
        return *get_impl();
    }
    stack_singleton() {
        set_instance(static_cast<base*>(this));
    }
    ~stack_singleton() {
        get_impl() = 0;
    }
};

inline void throw_system_failure(const char* msg)
{
    throw std::runtime_error(es() % (errno ? strerror(errno) : "") % ", " % msg);
}

template<typename type>
std::enable_if_t<std::is_integral<type>::value, std::string>
to_string(type value)
{
    char buf[24];
    uint32_t size = my_cvt::itoa(buf, value);
    return std::string(buf, buf + size);
}

inline std::string to_string(double value)
{
    char buf[32];
    uint32_t size = my_cvt::dtoa(buf, value);
    return std::string(buf, buf + size);
}

template<typename type>
std::enable_if_t<std::is_integral<type>::value, type>
lexical_cast(const char* from, const char* to)
{
    return my_cvt::atoi<type>(from, to - from);
}

template<typename type>
std::enable_if_t<!(std::is_integral<type>::value), type>
lexical_cast(const char* from, const char* to);

template<>
inline double lexical_cast<double>(const char* from, const char* to)
{
    char* ep;
    double ret;
    if(*to == char())
        ret = strtod(from, &ep);
    else {
        my_basic_string<char, 30> buf(from, to - from);
        ret = strtod(buf.c_str(), &ep);
        ep = (char*)(from + (ep - buf.begin()));
    }
    if(ep != to)
        throw std::runtime_error(es() % "bad lexical_cast to double from " % str_holder(from, to - from));
    return ret;
}

template<>
inline std::string lexical_cast<std::string>(const char* from, const char* to)
{
    return std::string(from, to);
}

template<typename type>
type lexical_cast(const std::string& str)
{
    return lexical_cast<type>(&str[0], &str[0] + str.size());
}

template<typename type>
type lexical_cast(std::string::const_iterator from, std::string::const_iterator to)
{
    return lexical_cast<type>(&(*from), &(*to));
}

template<typename type>
type lexical_cast(std::vector<char>::const_iterator from, std::vector<char>::const_iterator to)
{
    return lexical_cast<type>(&(*from), &(*to));
}

template<typename type>
type lexical_cast(const str_holder& str)
{
    return lexical_cast<type>(str.str, str.str + str.size);
}

static std::vector<std::string> split(const std::string& str, char sep = ',')
{
    std::vector<std::string> ret;
    auto it = str.begin(), ie = str.end(), i = it;
    while(it != ie) {
        i = std::find(it, ie, sep);
        ret.push_back(std::string(it, i));
        if(i != ie)
            ++i;
        it = i;
    }
    return ret;
}

static void split(std::vector<str_holder>& ret, const char* it, const char* ie, char sep = ',')
{
    while(it != ie) {
        const char* i = std::find(it, ie, sep);
        ret.push_back(str_holder(it, i - it));
        if(i != ie)
            ++i;
        it = i;
    }
}

class crc32_table : noncopyable
{
    uint32_t crc_table[256];
    crc32_table() {
        for(uint32_t i = 0; i != 256; ++i) {
            uint32_t crc = i;
            for (uint32_t j = 0; j != 8; j++)
                crc = crc & 1 ? (crc >> 1) ^ 0xedb88320ul : crc >> 1;
            crc_table[i] = crc;
        }
    }
public:
    static uint32_t* get() {
        static crc32_table t;
        return t.crc_table;
    }
};

struct crc32
{
    uint32_t *crc_table, crc;
    crc32(uint32_t init) : crc_table(crc32_table::get()), crc(init ^ 0xFFFFFFFFUL) {
    }
    void process_bytes(const char* p, uint32_t len) {
        const unsigned char* buf = reinterpret_cast<const unsigned char*>(p);
        while(len--)
            crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
    }
    uint32_t checksum() const {
        return crc ^ 0xFFFFFFFFUL;
    }
};

struct read_time_impl
{
    my_basic_string<char, 11> cur_date;
    uint64_t cur_date_time;
    template<uint32_t frac_size>
    ttime_t read_time(const char* &it)
    {
        //2020-01-26T10:45:21 //frac_size 0
        //2020-01-26T10:45:21.418 //frac_size 3
        //2020-01-26T10:45:21.418000001 //frac_size 9

        if(unlikely(cur_date != str_holder(it, 10)))
        {
            if(*(it + 4) != '-' || *(it + 7) != '-')
                throw std::runtime_error(es() % "bad time: " % std::string(it, it + 26));
            struct tm t = tm();
            int y = my_cvt::atoi<int>(it, 4); 
            int m = my_cvt::atoi<int>(it + 5, 2); 
            int d = my_cvt::atoi<int>(it + 8, 2); 
            t.tm_year = y - 1900;
            t.tm_mon = m - 1;
            t.tm_mday = d;
            cur_date_time = timegm(&t) * my_cvt::p10<9>();
            cur_date = str_holder(it, 10);
        }
        it += 10;
        if(*it != 'T' || *(it + 3) != ':' || *(it + 6) != ':' || (frac_size ? *(it + 9) != '.' : false))
            throw std::runtime_error(es() % "bad time: " % std::string(it - 10, it + 10 + (frac_size ? 1 + frac_size : 0)));
        uint64_t h = my_cvt::atoi<uint64_t>(it + 1, 2);
        uint64_t m = my_cvt::atoi<uint64_t>(it + 4, 2);
        uint64_t s = my_cvt::atoi<uint64_t>(it + 7, 2);
        uint64_t ns = 0;
        if(frac_size)
        {
            uint64_t frac = my_cvt::atoi<uint64_t>(it + 10, frac_size);
            ns = frac * my_cvt::p10<9 - frac_size>();
            it += (frac_size + 1);
        }
        it += 9;
        return ttime_t{cur_date_time + ns + (s + m * 60 + h * 3600) * my_cvt::p10<9>()};
    }
};

