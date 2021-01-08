/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "mlog.hpp"
#include "time.hpp"
#include "string.hpp"

#include <cassert>

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

void throw_system_failure(const std::string& msg);

template<typename type>
std::enable_if_t<std::is_integral<type>::value, std::string>
to_string(type value)
{
    char buf[24];
    uint32_t size = my_cvt::itoa(buf, value);
    return std::string(buf, buf + size);
}

std::string to_string(double value);

template<typename type>
std::enable_if_t<std::is_integral<type>::value, type>
lexical_cast(const char* from, const char* to)
{
    return my_cvt::atoi<type>(from, to - from);
}

template<typename type>
std::enable_if_t<!(std::is_integral<type>::value), type>
lexical_cast(const char* from, const char* to);

template<> double lexical_cast<double>(const char* from, const char* to);
template<> std::string lexical_cast<std::string>(const char* from, const char* to);

template<> inline str_holder lexical_cast<str_holder>(const char* from, const char* to)
{
    return str_holder(from, to - from);
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

std::vector<std::string> split(const std::string& str, char sep = ',');
void split(std::vector<str_holder>& ret, const char* it, const char* ie, char sep = ',');

struct crc32
{
    uint32_t *crc_table, crc;

    crc32(uint32_t init);
    void process_bytes(const char* p, uint32_t len);
    uint32_t checksum() const;
};

struct read_time_impl
{
    my_basic_string<11> cur_date;
    uint64_t cur_date_time;

    template<uint32_t frac_size>
    ttime_t read_time(const char* &it);
};

inline uint64_t get_decimal_pow(uint32_t e)
{
    if(e > 19)
        return std::numeric_limits<uint64_t>::max();
    else
        return my_cvt::decimal_pow[e];
}

template<typename decimal>
inline decimal read_decimal(const char* it, const char* ie)
{
    bool minus = (*it == '-');
    if(minus)
        ++it;
    const char* p = std::find(it, ie, '.');
    const char* E = std::find_if((p == ie ? it : p + 1), ie,
            [](char c) {
                return c == 'E' || c == 'e';
                });

    decimal ret;
    ret.value = my_cvt::atoi<int64_t>(it, std::min(p, E) - it);
    int digits = 0;
    int64_t _float = 0;

    if(p != ie) {
        ++p;
        digits = E - p;
        _float = my_cvt::atoi<int64_t>(p, digits);
    }
    int e = 0;
    if(E != ie) {
        ++E;
        e = my_cvt::atoi<int>(E, ie - E);
    }

    int em = -decimal::exponent + e;
    int fm = -decimal::exponent - digits + e;

    if(em < 0)
        ret.value /= get_decimal_pow(-em);
    else
        ret.value *= get_decimal_pow(em);

    if(fm < 0)
        _float /= get_decimal_pow(-fm);
    else
        _float *= get_decimal_pow(fm);
    ret.value += _float;

    if(minus)
        ret.value = -ret.value;
    return ret;
}

template<typename stream, typename decimal>
void write_decimal(stream& s, const decimal& d)
{
    int64_t int_ = d.value / decimal::frac;
    int32_t float_ = d.value % decimal::frac;
    if(d.value < 0) {
        s << '-';
        int_ = -int_;
        float_ = -float_;
    }
    s << int_ << "." << mlog_fixed<-decimal::exponent>(float_);
}

