/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "evie/time.hpp"
#include "evie/mlog.hpp"
#include "evie/utils.hpp"

template<typename stream>
stream& operator<<(stream& s, const price_t& p)
{
    if(p.value < 0)
        s << "-";
    s << (std::abs(p.value) / price_t::frac) << "." << mlog_fixed<5>(std::abs(p.value) % price_t::frac);
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const count_t& c)
{
    if(c.value < 0)
        s << "-";
    s << (std::abs(c.value) / count_t::frac) << "." << mlog_fixed<8>(std::abs(c.value) % count_t::frac);
    return s;
}

struct brief_time : ttime_t
{
    brief_time(ttime_t time) : ttime_t(time)
    {
    }
};

template<typename stream>
stream& operator<<(stream& s, const brief_time& t)
{
    time_t tt = t.value / ttime_t::frac;
    tt = tt % (3600 * 24);
    uint32_t h = tt / 3600;
    uint32_t m = (tt - h * 3600) / 60;
    s << print2chars(h) << ':' << print2chars(m) << ':' << print2chars(tt % 60) << '.' << t.value % ttime_t::frac;
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_trade& t)
{
    s << "trade|"<< t.security_id << "|" << t.direction << "|"
        << t.price << "|" << t.count
        << "|" << t.etime << "|" << t.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_instr& i)
{
    s << "instr|"<< i.exchange_id << "|" << i.feed_id << "|" << i.security << "|" << i.security_id
        << "|" << i.etime << "|" << i.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_clean& c)
{
    s << "clean|"<< c.security_id << "|" << c.source
        << "|" << c.etime << "|" << c.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_book& t)
{
    s << "book|"<< t.security_id << "|" << t.level_id << "|" << t.price << "|" << t.count
        << "|" << t.etime << "|" << t.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_ping& p)
{
    s << "ping|"
        << p.etime << "|" << p.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_hello& h)
{
    s << "ping|" << h.name
        << "|" << h.etime << "|" << h.time << "|";
    return s;
}

inline bool operator<(price_t l, price_t r)
{
    return l.value < r.value;
}

inline bool operator>(price_t l, price_t r)
{
    return l.value > r.value;
}

inline bool operator!=(price_t l, price_t r)
{
    return l.value != r.value;
}

inline bool operator==(price_t l, price_t r)
{
    return l.value == r.value;
}

inline bool operator<(count_t l, count_t r)
{
    return l.value < r.value;
}

inline bool operator>(count_t l, count_t r)
{
    return l.value > r.value;
}

inline bool operator!=(count_t l, count_t r)
{
    return l.value != r.value;
}

inline bool operator==(count_t l, count_t r)
{
    return l.value == r.value;
}

inline uint64_t get_decimal_pow(uint32_t e)
{
    if(e > 19)
        return std::numeric_limits<int64_t>::max();
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

inline price_t read_price(const char* it, const char* ie)
{
    return read_decimal<price_t>(it, ie);
}

inline count_t read_count(const char* it, const char* ie)
{
    return read_decimal<count_t>(it, ie);
}

inline uint32_t calc_crc(const message_instr& i)
{
    crc32 crc(0);
    crc.process_bytes(i.exchange_id, sizeof(i.exchange_id) - 1);
    crc.process_bytes(i.feed_id, sizeof(i.feed_id) - 1);
    crc.process_bytes(i.security, sizeof(i.security) - 1);
    return crc.checksum();
}

