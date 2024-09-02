/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "alco.hpp"

#include "../makoa/types.hpp"

void security::proceed_book(exporter& e, price_t price, count_t count, ttime_t etime, ttime_t time)
{
    mb.time = time;
    mb.etime = etime;
    mb.level_id = price.value;
    mb.price = price;
    mb.count = count;
    _.t.time = cur_ttime(); //get_export_mtime
    e.proceed((message*)(&mb), 1);
}

void security::proceed_trade(exporter& e, uint32_t direction, price_t price, count_t count, ttime_t etime, ttime_t time)
{
    mt.time = time;
    mt.etime = etime;
    mt.direction = direction;
    mt.price = price;
    mt.count = count;
    mb.time = cur_ttime(); //get_export_mtime
    e.proceed((message*)(&mt), 1);
}

void security::proceed_clean(exporter& e)
{
    mc.time = cur_ttime();
    mt.time = ttime_t(); //get_export_mtime
    e.proceed((message*)(&mc), 1);
}

void security::proceed_instr(exporter& e, ttime_t time)
{
    mi.time = time;
    mc.time = ttime_t(); //get_export_mtime
    e.proceed((message*)(&mi), 1);
}

void security::proceed_ping(exporter& e, ttime_t etime)
{
    mp.etime = etime;
    mp.time = cur_ttime();
    mi.time = ttime_t(); //get_export_mtime
    e.proceed((message*)(&mp), 1);
}

void security::init(const mstring& exchange_id, const mstring& feed_id, const mstring& ticker)
{
    mb = message_book();
    mb.id = msg_book;
    mt = message_trade();
    mt.id = msg_trade;
    mc = message_clean();
    mc.id = msg_clean;
    mi = message_instr();
    mi.id = msg_instr;
    mp = message_ping();
    mp.id = msg_ping;

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
    mi.time = cur_ttime();

    mb.security_id = mi.security_id;
    mt.security_id = mi.security_id;
    mc.security_id = mi.security_id;
}

emessages::emessages(const mstring& push) : e(push), m_s()
{
}

void emessages::ping(ttime_t etime, ttime_t time)
{
    if(m_s != pre_alloc) {
        message_ping& p = ms[m_s++].mp;
        p.time = time;
        p.etime = etime;
        p.id = msg_ping;
    }
    send_messages();
}

void emessages::add_clean(uint32_t security_id, ttime_t etime, ttime_t time)
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

void emessages::add_order(uint32_t security_id, int64_t level_id, price_t price, count_t count, ttime_t etime, ttime_t time)
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

void emessages::add_trade(uint32_t security_id, price_t price, count_t count, uint32_t direction, ttime_t etime, ttime_t time)
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
    if(m_s) {
        set_export_mtime(ms);
        e.proceed(ms, m_s);
        m_s = 0;
    }
}

