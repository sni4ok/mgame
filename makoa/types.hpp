/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "evie/time.hpp"
#include "evie/mlog.hpp"

static const uint32_t price_frac = 100000;
static const uint32_t count_frac = 100000000;

template<typename stream>
stream& operator<<(stream& s, const price_t& p)
{
    if(p.value < 0)
        s << "-";
    s << (std::abs(p.value) / price_frac) << "." << mlog_fixed<5>(std::abs(p.value) % price_frac);
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const count_t& c)
{
    if(c.value < 0)
        s << "-";
    s << (std::abs(c.value) / count_frac) << "." << mlog_fixed<8>(std::abs(c.value) % count_frac);
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
    char buf[20];
    time_t tt = t.value / 1000000;
    strftime(buf, 20, "%H:%M:%S", localtime(&tt));
    s << buf << "." << mlog_fixed<6>(t.value % 1000000);
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_trade& t)
{
    s << "trade|"<< t.security_id << "|" << t.direction << "|"
        << t.price << "|" << t.count << "|" << t.etime
        << "|" << t.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_instr& i)
{
    s << "instr|"<< i.exchange_id << "|" << i.feed_id << "|" << i.security << "|"
        << i.security_id << "|" << i.time;
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_clean& c)
{
    s << "clean|"<< c.security_id << "|" << c.source << "|" << c.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_book& t)
{
    s << "book|"<< t.security_id << "|" << t.price << "|" << t.count << "|" << t.time << "|";
    return s;
}

inline bool operator<(price_t l, price_t r)
{
    return l.value < r.value;
}

inline bool operator!=(price_t l, price_t r)
{
    return l.value != r.value;
}
