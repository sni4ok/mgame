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

struct uint_fix
{
    uint64_t value;
    uint32_t size;
    bool zeros;

    constexpr str_holder str() const
    {
        assert(size);
        uint64_t v = value;
        uint32_t idx = size - 1;

        while(v > 9 && idx)
        {
            v /= 10;
            --idx;
        }

        if(zeros)
            return str_holder("0000000000000", idx);
        else
            return str_holder("             ", idx);
    }
};

template<uint32_t sz, bool zero = true>
struct uint_fixed : uint_fix
{
    uint_fixed(uint64_t value) : uint_fix(value, sz, zero)
    {
        static_assert(sz > 0 && sz <= 13, "out of range");
    }
};

template<typename stream>
stream& operator<<(stream& log, const uint_fix& v)
{
    log << v.str() << v.value;
    return log;
}

typedef uint_fixed<2> print2chars;

template<typename type>
void split(mvector<type>& ret, char_cit it, char_cit ie, char sep)
{
    if(it != ie)
    {
        for(;;)
        {
            char_cit i = find(it, ie, sep);
            ret.push_back(type(it, i));

            if(i == ie)
                break;

            it = i + 1;
        }
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

template<typename stream, typename decimal>
stream& operator<<(stream& s, decimal d)
    requires(is_decimal<decimal>)
{
    decltype(decimal::value) int_ = d.value / decimal::frac;
    decltype(decimal::value) float_ = d.value % decimal::frac;

    if(d.value < 0)
    {
        s << '-';
        int_ = -int_;
        float_ = -float_;
    }

    s << int_ << "." << uint_fixed<-decimal::exponent>(float_);
    return s;
}

struct counting_iterator
{
    int64_t value;

    counting_iterator(int64_t value) : value(value)
    {
    }
    int64_t operator-(counting_iterator r) const
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
    bool operator==(counting_iterator r) const
    {
        return value == r.value;
    }
    bool operator!=(counting_iterator r) const
    {
        return value != r.value;
    }
    int64_t operator*() const
    {
        return value;
    }
};

typedef pair<counting_iterator, counting_iterator> pcc;

struct zpcc : pcc
{
    zpcc(int64_t value) : pcc(0, value)
    {
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
    for(iterator it = p.from; it != p.to; ++it)
    {
        if constexpr(no_end_sep && sep)
        {
            if(it != p.from)
                s << sep;
        }

        p.f(s, *it);
        if constexpr(!no_end_sep && sep)
            s << sep;
    }
    return s;
}

struct print_default
{
    template<typename stream, typename type>
    stream& operator()(stream& s, const type& v) const
    {
        return s << v;
    }
};

template<char sep = ',', bool no_end_sep = true, typename iterator, typename func = print_default>
print_impl<sep, no_end_sep, iterator, func> print(iterator from, iterator to, func f = func())
{
    return {from, to, f};
}

template<char sep = ',', typename cont, typename func = print_default>
auto print(const cont& c, func f = func())
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
stream& operator<<(stream& s, date d)
{
    if(d == cur_day_date)
        s << cur_day_date_str;
    else
        s << d.year << '-' << print2chars(d.month) << '-' << print2chars(d.day);
    return s;
}

template<typename stream>
stream& operator<<(stream& s, time_duration t)
{
    return s << print2chars(t.hours) << ':' << print2chars(t.minutes) << ':' << print2chars(t.seconds)
        << "." << uint_fixed<6>(t.nanos / 1000);
}

template<typename stream>
stream& operator<<(stream& s, time_parsed p)
{
    return s << p.date << ' ' << p.duration;
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

