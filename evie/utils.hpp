/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "mlog.hpp"
#include "string.hpp"
#include "mstring.hpp"
#include "algorithm.hpp"

#include <cassert>

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

template<typename type>
type lexical_cast(const str_holder& str)
{
    return lexical_cast<type>(str.begin(), str.end());
}

mvector<mstring> split(const mstring& str, char sep = ',');
mvector<str_holder> split(char_cit it, char_cit ie, char sep = ',');
void split(mvector<str_holder>& ret, char_cit it, char_cit ie, char sep = ',');
mstring join(mvector<mstring>::const_iterator it, mvector<mstring>::const_iterator ie, char sep = ',');
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
    my_basic_string<11> cur_date;
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
    bool operator!=(const counting_iterator& r) const
    {
        return value != r.value;
    }
    int64_t operator*() const
    {
        return value;
    }
};

template<typename f, typename s>
struct pair
{
    f first;
    s second;

    f begin() const
    {
        return first;
    }
    f end() const
    {
        return second;
    }
};

template<typename type>
bool operator<(const mvector<type>& l, const mvector<type>& r)
{
    return lexicographical_compare(l.begin(), l.end(), r.begin(), r.end());
}

