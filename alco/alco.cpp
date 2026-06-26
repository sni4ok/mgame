/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "alco.hpp"

#include "../evie/profiler.hpp"
#include "../makoa/types.hpp"

u32 emessages::proceed_instr(str_holder exchange_id, str_holder feed_id, str_holder ticker, ttime_t time)
{
    if(m_s == pre_alloc) [[unlikely]]
        send_messages();

    message_instr& mi = ms[m_s++].mi;

    mi.time = time;
    mi.etime = ttime_t();
    mi.id = msg_instr;

    memset(&mi.exchange_id, 0, sizeof(mi.exchange_id));
    memset(&mi.feed_id, 0, sizeof(mi.feed_id));
    memset(&mi.security, 0, sizeof(mi.security));

    if(exchange_id.size() > sizeof(mi.exchange_id))
        throw mexception(es() % "exchange_id too big: " % exchange_id);
    copy(exchange_id.begin(), exchange_id.end(), &mi.exchange_id[0]);

    if(feed_id.size() > sizeof(mi.feed_id))
        throw mexception(es() % "feed_id too big: " % feed_id);
    copy(feed_id.begin(), feed_id.end(), &mi.feed_id[0]);

    if(ticker.size() > sizeof(mi.security))
        throw mexception(es() % "security too big: " % ticker);
    copy(ticker.begin(), ticker.end(), &mi.security[0]);

    mi.security_id = calc_crc(mi);
    return mi.security_id;
}

emessages::emessages(const mstring& push) : e(push), m_s()
{
}

void emessages::ping(ttime_t etime, ttime_t time)
{
    if(m_s != pre_alloc)
    {
        message_ping& p = ms[m_s++].mp;
        p.time = time;
        p.etime = etime;
        p.id = msg_ping;
    }
    send_messages();
}

void emessages::add_clean(u32 security_id, ttime_t etime, ttime_t time)
{
    if(m_s == pre_alloc) [[unlikely]]
        send_messages();

    message_clean& c = ms[m_s++].mc;
    c.time = time;
    c.etime = etime;
    c.id = msg_clean;
    c.security_id = security_id;
    c.source = 0;
}

void emessages::add_order(u32 security_id, i64 level_id, price_t price, count_t count,
    ttime_t etime, ttime_t time)
{
    if(m_s == pre_alloc) [[unlikely]]
        send_messages();

    message_book& m = ms[m_s++].mb;
    m.time = time;
    m.etime = etime;
    m.id = msg_book;
    m.security_id = security_id;
    m.level_id = level_id;
    m.price = price;
    m.count = count;
}

void emessages::add_trade(u32 security_id, price_t price, count_t count, u32 direction,
    ttime_t etime, ttime_t time)
{
    if(m_s == pre_alloc) [[unlikely]]
        send_messages();

    message_trade& m = ms[m_s++].mt;
    m.time = time;
    m.etime = etime;
    m.id = msg_trade;
    m.direction = direction;
    m.security_id = security_id;
    m.price = price;
    m.count = count;
}

void emessages::send_messages()
{
    if(m_s)
    {
        e.proceed(ms, m_s);
        MPROFILE_COUNT("emessages::m_s", {m_s})
        m_s = 0;
    }
}

