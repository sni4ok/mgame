/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "../evie/time.hpp"
#include "../evie/utils.hpp"

struct print_date
{
    date value;
    char sep;

    explicit print_date(date value, char sep = '-')
        : value(value), sep(sep)
    {
    }
};

struct print_td
{
    enum mode_t
    {
        sec = 0, ms, us, ns
    };

    time_duration value;
    int mode;

    explicit print_td(time_duration value, int mode = sec)
        : value(value), mode(mode)
    {
    }
    explicit print_td(ttime_t time, int mode = sec)
        : print_td(get_time_duration(time), mode)
    {
    }
};

struct print_time
{
    ttime_t value;
    char sep;
    int mode;

    explicit print_time(ttime_t value, char sep = ' ', int mode = print_td::sec)
        : value(value), sep(sep), mode(mode)
    {
    }
};

template<typename stream>
stream& operator<<(stream& log, print_date d)
{
    log << u32(d.value.year) << d.sep << print2chars(d.value.month)
        << d.sep << print2chars(d.value.day);
    return log;
}

template<typename stream>
stream& operator<<(stream& log, print_td t)
{
    log << print2chars(t.value.hours) << ':' << print2chars(t.value.minutes)
        << ':' << print2chars(t.value.seconds);
    if(t.mode == print_td::ns)
        log << "." << uint_fixed<9>(t.value.nanos);
    else if(t.mode == print_td::us)
        log << "." << uint_fixed<6>(t.value.nanos / 1000);
    else if(t.mode == print_td::ms)
        log << "." << uint_fixed<3>(t.value.nanos / pow10_v<6>);
    return log;
}

template<typename stream>
stream& operator<<(stream& log, print_time t)
{
    time_parsed p = parse_time(t.value);
    log << print_date(p.date) << t.sep << print_td(p.duration, t.mode);
    return log;
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
    s << "instr|" << i.id << "|" << i.exchange_id << "|" << i.feed_id << "|" << i.security
        << "|" << i.security_id << "|" << i.etime << "|" << i.time << "|";
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
    s << "book|" << t.id << "|" << t.security_id << "|" << t.level_id << "|"
        << t.price << "|" << t.count << "|" << t.etime << "|" << t.time << "|";
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

