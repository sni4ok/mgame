/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "../evie/time.hpp"
#include "../evie/utils.hpp"

struct brief_time : ttime_t
{
    bool show_frac;
    explicit brief_time(ttime_t time, bool show_frac = true) : ttime_t(time), show_frac(show_frac)
    {
    }
};

template<typename stream>
stream& operator<<(stream& s, const brief_time& t)
{
    time_t tt = t.value / ttime_t::frac;
    tt = tt % (3600 * 24);
    u32 h = tt / 3600;
    u32 m = (tt - h * 3600) / 60;
    s << print2chars(h) << ':' << print2chars(m) << ':' << print2chars(tt % 60);
    if(t.show_frac)
        s << '.' << uint_fixed<9>(t.value % ttime_t::frac);
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_trade& t)
{
    s << "trade|" << t.id << "|" << t.security_id << "|" << t.direction << "|"
        << t.price << "|" << t.count
        << "|" << t.etime << "|" << t.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_instr& i)
{
    s << "instr|" << i.id << "|" << i.exchange_id << "|" << i.feed_id << "|" << i.security << "|" << i.security_id
        << "|" << i.etime << "|" << i.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_clean& c)
{
    s << "clean|" << c.id << "|" << c.security_id << "|" << c.source
        << "|" << c.etime << "|" << c.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_book& t)
{
    s << "book|" << t.id << "|" << t.security_id << "|" << t.level_id << "|" << t.price << "|" << t.count
        << "|" << t.etime << "|" << t.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_ping& p)
{
    s << "ping|" << p.id << "|"
        << p.etime << "|" << p.time << "|";
    return s;
}

template<typename stream>
stream& operator<<(stream& s, const message_hello& h)
{
    s << "ping|" << h.name << h.id << "|"
        << "|" << h.etime << "|" << h.time << "|";
    return s;
}

inline u32 calc_crc(const message_instr& i)
{
    crc32 crc(0);
    crc.process_bytes(i.exchange_id, sizeof(i.exchange_id) - 1);
    crc.process_bytes(i.feed_id, sizeof(i.feed_id) - 1);
    crc.process_bytes(i.security, sizeof(i.security) - 1);
    return crc.checksum();
}

