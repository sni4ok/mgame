/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_UTILS_HPP
#define EVIE_UTILS_HPP

#include "mlog.hpp"
#include "string.hpp"
#include "mstring.hpp"
#include "algorithm.hpp"
#include "array.hpp"

struct noncopyable
{
    noncopyable() {
    }
private:
    noncopyable(const noncopyable&) = delete;
};

template<> inline str_holder lexical_cast<str_holder>(char_cit from, char_cit to)
{
    return str_holder(from, to - from);
}

mvector<mstring> split(const mstring& str, char sep = ',');
mvector<str_holder> split(str_holder str, char sep = ',');
void split(mvector<str_holder>& ret, char_cit it, char_cit ie, char sep = ',');

mstring join(const mvector<mstring>& s, char sep = ',');
mstring join(const mstring* it, const mstring* ie, char sep = ',');

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
    s << int_ << "." << mlog_fixed<-decimal::exponent>(float_);
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

template<typename type>
bool operator<(const mvector<type>& l, const mvector<type>& r)
{
    return lexicographical_compare(l.begin(), l.end(), r.begin(), r.end());
}

template<typename iterator, typename func, char s, bool no_end_s>
struct print_impl
{
    static const char sep = s;
    static const bool no_end_sep = no_end_s;

    iterator from, to;
    func f;
};

template<typename stream, typename iterator, typename func, char sep, bool no_end_sep>
stream& operator<<(stream& s, const print_impl<iterator, func, sep, no_end_sep>& p)
{
    iterator it = p.from;
    for(; it != p.to; ++it)
    {
        if constexpr(no_end_sep)
        {
            if(it != p.from)
                s << p.sep;
        }
        p.f(s, *it);
        if constexpr(!no_end_sep)
            s << p.sep;
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
print_impl<iterator, func, sep, no_end_sep> print(iterator from, iterator to, func f = func())
{
    return {from, to, f};
}

template<char sep = ',', typename cont, typename func = print_default>
auto print(const cont& c, func f = func())
{
    if constexpr(std::is_array<cont>::value)
        return print(std::begin(c), std::end(c), f);
    else
        return print(c.begin(), c.end(), f);
}

template<char sep = '\n', typename cont, typename func = print_default>
auto print_csv(const cont& c, func f = func())
{
    if constexpr(std::is_array<cont>::value)
        return print<sep, false>(std::begin(c), std::end(c), f);
    else
        return print<sep, false>(c.begin(), c.end(), f);
}

#endif

