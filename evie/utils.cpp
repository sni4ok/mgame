/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "utils.hpp"
#include "rational.hpp"
#include "math.h"

#include <errno.h>
#include <string.h>

#include <numeric>

void throw_system_failure(str_holder msg)
{
    throw mexception(es() % _str_holder((errno ? strerror(errno) : "")) % ", " % msg);
}

str_holder _str_holder(char_cit str)
{
    return str_holder(str, strlen(str));
}

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

mvector<mstring> split_s(str_holder str, char sep)
{
    mvector<mstring> ret;
    split(ret, str.begin(), str.end(), sep);
    return ret;
}

mvector<str_holder> split(str_holder str, char sep)
{
    mvector<str_holder> ret;
    split(ret, str.begin(), str.end(), sep);
    return ret;
}

mstring join(const mstring* it, const mstring* ie, char sep)
{
    if(it == ie)
        return mstring();

    uint32_t sz = 0;
    for(auto v = it; v != ie; ++v)
        sz += v->size();

    mstring ret;
    ret.resize(sz + (ie - it) - 1);
    buf_stream str(&ret[0], &ret[0] + ret.size());

    for(auto v = it; v != ie; ++v) {
        if(v != it)
            str << sep;
        str << *v;
    }

    return ret;
}

mstring join(const mvector<mstring>& s, char sep)
{
    return join(s.begin(), s.end(), sep);
}

class crc32_table
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

    crc32_table(const crc32_table&) = delete;

public:
    static uint32_t* get() {
        static crc32_table t;
        return t.crc_table;
    }
};

crc32::crc32(uint32_t init) : crc_table(crc32_table::get()), crc(init ^ 0xFFFFFFFFUL)
{
}

void crc32::process_bytes(char_cit p, uint32_t len)
{
    const unsigned char* buf = reinterpret_cast<const unsigned char*>(p);
    while(len--)
        crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
}

uint32_t crc32::checksum() const
{
    return crc ^ 0xFFFFFFFFUL;
}

template<uint32_t frac_size>
ttime_t read_time_impl::read_time(char_cit& it)
{
    //2020-01-26T10:45:21 //frac_size 0
    //2020-01-26T10:45:21.418 //frac_size 3
    //2020-01-26T10:45:21.418000001 //frac_size 9

    if(!cur_date.equal(it)) [[unlikely]]
    {
        if(*(it + 4) != '-' || *(it + 7) != '-')
            throw mexception(es() % "bad time: " % str_holder(it, 26));
        struct tm t = tm();
        int y = my_cvt::atoi<int>(it, 4); 
        int m = my_cvt::atoi<int>(it + 5, 2); 
        int d = my_cvt::atoi<int>(it + 8, 2); 
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        cur_date_time = timegm(&t) * my_cvt::p10<9>();
        cur_date.init(it);
    }
    it += 10;
    if(*it != 'T' || *(it + 3) != ':' || *(it + 6) != ':' || (frac_size ? *(it + 9) != '.' : false))
        throw mexception(es() % "bad time: " % str_holder(it - 10, 20 + (frac_size ? 1 + frac_size : 0)));
    uint64_t h = my_cvt::atoi<uint64_t>(it + 1, 2);
    uint64_t m = my_cvt::atoi<uint64_t>(it + 4, 2);
    uint64_t s = my_cvt::atoi<uint64_t>(it + 7, 2);
    uint64_t ns = 0;
    if constexpr(frac_size)
    {
        uint64_t frac = my_cvt::atoi<uint64_t>(it + 10, frac_size);
        ns = frac * my_cvt::p10<9 - frac_size>();
        it += (frac_size + 1);
    }
    it += 9;
    return ttime_t{cur_date_time + ns + (s + m * 60 + h * 3600) * ttime_t::frac};
}

template ttime_t read_time_impl::read_time<0>(char_cit&);
template ttime_t read_time_impl::read_time<3>(char_cit&);
template ttime_t read_time_impl::read_time<6>(char_cit&);
template ttime_t read_time_impl::read_time<9>(char_cit&);

namespace
{
    time_parsed parse_time_impl(const ttime_t& time)
    {
        time_parsed ret;
        time_t ti = time.value / ttime_t::frac;
        struct tm * t = gmtime(&ti);
        ret.year = t->tm_year + 1900;
        ret.month = t->tm_mon + 1;
        ret.day = t->tm_mday;
        ret.hours = t->tm_hour;
        ret.minutes = t->tm_min;
        ret.seconds = t->tm_sec;
        ret.nanos = time.value % ttime_t::frac;
        return ret;
    }

}

const uint32_t cur_day_seconds = day_seconds(cur_mtime_seconds());
const date cur_day_date = parse_time_impl(cur_mtime_seconds());

inline my_string get_cur_day_str()
{
    buf_stream_fixed<20> str;
    str << mlog_fixed<4>(cur_day_date.year) << "-" << mlog_fixed<2>(cur_day_date.month) << "-" << mlog_fixed<2>(cur_day_date.day);
    return my_string(str.begin(), str.end());
}

const my_string cur_day_date_str = get_cur_day_str();

time_parsed parse_time(const ttime_t& time)
{
    if(day_seconds(time) == cur_day_seconds)
    {
        time_parsed ret;
        ret.date() = cur_day_date;
        uint32_t frac = (time.value / ttime_t::frac) % (24 * 3600);
        ret.seconds = frac % 60;
        ret.hours = frac / 3600;
        ret.minutes = (frac - ret.hours * 3600) / 60;
        ret.nanos = time.value % ttime_t::frac;
        return ret;
    }
    else
        return parse_time_impl(time);
}

time_duration get_time_duration(const ttime_t& time)
{
    time_duration ret;
    uint32_t frac = (time.value / time.frac) % (24 * 3600);
    ret.seconds = frac % 60;
    ret.hours = frac / 3600;
    ret.minutes = (frac - ret.hours * 3600) / 60;
    ret.nanos = time.value % time.frac;
    return ret;
}

ttime_t pack_time(const time_parsed& p)
{
    struct tm t = tm();
    t.tm_year = p.year - 1900;
    t.tm_mon = p.month - 1;
    t.tm_mday = p.day;
    t.tm_hour = p.hours;
    t.tm_min = p.minutes;
    t.tm_sec = p.seconds;
    return {uint64_t(timegm(&t) * ttime_t::frac + p.nanos)};
}

date& date::operator+=(date_duration d)
{
    time_parsed tp = time_parsed();
    tp.date() = *this;
    ttime_t t = pack_time(tp);
    t.value += int64_t(d.days) * 24 * 3600 * ttime_t::frac;
    tp = parse_time(t);
    *this = tp.date();
    return *this;
}

date_duration date::operator-(const date& r) const
{
    time_parsed t = time_parsed();
    t.date() = *this;
    ttime_t tl = pack_time(t);
    t.date() = r;
    ttime_t tr = pack_time(t);
    int64_t ns = tl - tr;
    return date_duration(ns / ttime_t::frac / (24 * 3600));
}

ttime_t time_from_date(const date& t)
{
    time_parsed p = time_parsed();
    p.date() = t;
    return pack_time(p);
}

inline uint64_t get_decimal_pow(uint32_t e)
{
    if(e > 19)
        return limits<uint64_t>::max;
    else
        return my_cvt::decimal_pow[e];
}

int64_t read_decimal_impl(char_cit it, char_cit ie, int exponent)
{
    bool minus = (*it == '-');
    if(minus)
        ++it;
    char_cit p = find(it, ie, '.');
    char_cit E = find_if((p == ie ? it : p + 1), ie,
            [](char c) {
                return c == 'E' || c == 'e';
                });

    int64_t ret = my_cvt::atoi<int64_t>(it, min(p, E) - it);
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

    int em = -exponent + e;
    int fm = -exponent - digits + e;

    if(em < 0)
        ret /= get_decimal_pow(-em);
    else
        ret *= get_decimal_pow(em);

    if(fm < 0)
        _float /= get_decimal_pow(-fm);
    else
        _float *= get_decimal_pow(fm);
    ret += _float;

    if(minus)
        ret = -ret;
    return ret;
}

r& r::operator+=(const r& v)
{
    uint32_t lcm = std::lcm(den, v.den);
    num = num * (lcm / den) + v.num * (lcm / v.den);
    den = lcm;
    return *this;
}

r r::operator*(const r& v) const
{
    int64_t n = num * v.num;
    uint64_t d = den * v.den;
    int64_t gcd = int64_t(std::gcd(d, abs(n)));
    return r(int32_t(n / gcd), uint32_t(d / gcd));
}

mstring to_string(r value)
{
    buf_stream_fixed<24> str;
    str << value;
    return str.str();
}

template<> r lexical_cast<r>(char_cit from, char_cit to)
{
    char_cit i = find(from, to, '/');
    int32_t cur_n = lexical_cast<int32_t>(from, i);
    int32_t cur_d = 1;
    if(i != to)
        cur_d = lexical_cast<int32_t>(i + 1, to);
    return r(cur_n, cur_d);
}

