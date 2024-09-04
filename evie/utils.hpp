/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_UTILS_HPP
#define EVIE_UTILS_HPP

#include "string.hpp"
#include "mstring.hpp"
#include "algorithm.hpp"
#include "array.hpp"
#include "mtime.hpp"

static constexpr str_holder uint_fixed_str[] =
{
   "", "0", "00",
   "000", "0000", "00000",
   "000000", "0000000", "00000000"
};

template<uint32_t sz>
struct uint_fixed
{
   const uint32_t value;

   uint_fixed(uint32_t value) : value(value)
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
      return uint_fixed_str[idx];
   }
};

template<typename stream, uint32_t sz>
stream& operator<<(stream& log, const uint_fixed<sz>& v)
{
    log << v.str() << v.value;
    return log;
}

typedef uint_fixed<2> print2chars;

template<typename type>
void split(mvector<type>& ret, char_cit it, char_cit ie, char sep)
{
    while(it != ie) {
        char_cit i = find(it, ie, sep);
        ret.push_back(type(it, i));
        if(i != ie)
            ++i;
        it = i;
    }
}

mvector<mstring> split_s(str_holder str, char sep = ',');
mvector<str_holder> split(str_holder str, char sep = ',');

mstring join(const mstring* it, const mstring* ie, char sep = ',');
mstring join(const mvector<mstring>& s, char sep = ',');

struct crc32
{
    uint32_t *crc_table, crc;

    crc32(uint32_t init);
    void process_bytes(char_cit p, uint32_t len);
    uint32_t checksum() const;
};

struct read_time_impl
{
    carray<char, 10> cur_date;
    uint64_t cur_date_time;

    template<uint32_t frac_size>
    ttime_t read_time(char_cit& it);
};

int64_t read_decimal_impl(char_cit it, char_cit ie, int exponent);

template<typename decimal>
inline decimal read_decimal(char_cit it, char_cit ie)
{
    decimal ret;
    ret.value = read_decimal_impl(it, ie, decimal::exponent);
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
    s << int_ << "." << uint_fixed<-decimal::exponent>(float_);
}

struct counting_iterator
{
    int64_t value;

    counting_iterator(int64_t value) : value(value)
    {
    }
    int64_t operator-(const counting_iterator& r) const
    {
        return value - r.value;
    }
    counting_iterator& operator++()
    {
        ++value;
        return *this;
    }
    counting_iterator operator+(int64_t v) const
    {
        return counting_iterator(value + v);
    }
    bool operator==(const counting_iterator& r) const
    {
        return value == r.value;
    }
    bool operator!=(const counting_iterator& r) const
    {
        return value != r.value;
    }
    int64_t operator*() const
    {
        return value;
    }
};

template<char s, bool no_end_s, typename iterator, typename func>
struct print_impl
{
    iterator from, to;
    func f;
};

template<typename stream, char sep, bool no_end_sep, typename iterator, typename func>
stream& operator<<(stream& s, const print_impl<sep, no_end_sep, iterator, func>& p)
{
    iterator it = p.from;
    for(; it != p.to; ++it)
    {
        if constexpr(no_end_sep)
        {
            if(it != p.from)
                s << sep;
        }
        p.f(s, *it);
        if constexpr(!no_end_sep)
            s << sep;
    }
    return s;
}

struct print_default
{
    template<typename stream, typename type>
    void operator()(stream& s, const type& v) const
    {
        s << v;
    }
};

template<char sep = ',', char no_end_sep = true, typename iterator, typename func = print_default>
print_impl<sep, no_end_sep, iterator, func> print(iterator from, iterator to, func f = func())
{
    return {from, to, f};
}

template<char sep = ',', typename cont, typename func = print_default>
auto print(const cont& c, func f = func())
    requires(!__have_tuple_size<cont>)
{
    return print<sep>(::begin(c), ::end(c), f);
}

template<char sep = '\n', typename cont, typename func = print_default>
auto print_csv(const cont& c, func f = func())
{
    return print<sep, false>(::begin(c), ::end(c), f);
}

extern const date cur_day_date;
extern const my_string cur_day_date_str;

template<typename stream>
stream& operator<<(stream& s, const date& d)
{
    if(d == cur_day_date)
        s << cur_day_date_str;
    else
        s << d.year << '-' << print2chars(d.month) << '-' << print2chars(d.day);
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const time_duration& t)
{
    return s << print2chars(t.hours) << ':' << print2chars(t.minutes) << ':' << print2chars(t.seconds)
        << "." << uint_fixed<6>(t.nanos / 1000);
}

template<typename stream>
stream& operator<<(stream& s, const time_parsed& p)
{
    return s << p.date() << ' ' << p.duration();
}

template<typename stream>
stream& operator<<(stream& s, ttime_t v)
{
    return s << parse_time(v);
}

str_holder itoa_hex(uint8_t ch);

inline auto print_binary(str_holder str)
{
    auto f = [](auto& s, char c)
    {
        s << itoa_hex((uint8_t)c);
    };

    return print<' '>(str, f);
}

#endif

