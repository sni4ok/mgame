/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/types.hpp"
#include "makoa/exports.hpp"

struct security
{
    message _;
    message_book mb;
    message_trade mt;
    message_clean mc;
    message_instr mi;
    message_ping mp;

    void proceed_book(exporter& e, price_t price, count_t count, ttime_t etime, ttime_t time)
    {
        mb.time = time;
        mb.etime = etime;
        mb.level_id = price.value;
        mb.price = price;
        mb.count = count;
        _.t.time = get_cur_ttime(); //get_export_mtime
        e.proceed((message*)(&mb), 1);
    }
    void proceed_trade(exporter& e, uint32_t direction, price_t price, count_t count, ttime_t etime, ttime_t time)
    {
        mt.time = time;
        mt.etime = etime;
        mt.direction = direction;
        mt.price = price;
        mt.count = count;
        mb.time = get_cur_ttime(); //get_export_mtime
        e.proceed((message*)(&mt), 1);
    }
    void proceed_clean(exporter& e)
    {
        mc.time = get_cur_ttime();
        mt.time = ttime_t(); //get_export_mtime
        e.proceed((message*)(&mc), 1);
    }
    void proceed_instr(exporter& e, ttime_t time)
    {
        mi.time = time;
        mc.time = ttime_t(); //get_export_mtime
        e.proceed((message*)(&mi), 1);
    }
    void proceed_ping(exporter& e, ttime_t etime)
    {
        mp.etime = etime;
        mp.time = get_cur_ttime();
        mi.time = ttime_t(); //get_export_mtime
        e.proceed((message*)(&mp), 1);
    }
    void init(const std::string& exchange_id, const std::string& feed_id, const std::string& ticker)
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
            throw std::runtime_error(es() % "exchange_id too big: " % exchange_id);
        std::copy(exchange_id.begin(), exchange_id.end(), &mi.exchange_id[0]);

        if(feed_id.size() > sizeof(mi.feed_id))
            throw std::runtime_error(es() % "feed_id too big: " % feed_id);
        std::copy(feed_id.begin(), feed_id.end(), &mi.feed_id[0]);
        
        if(ticker.size() > sizeof(mi.security))
            throw std::runtime_error(es() % "security too big: " % ticker);
        std::copy(ticker.begin(), ticker.end(), &mi.security[0]);

        mi.security_id = calc_crc(mi);
        mi.time = get_cur_ttime();

        mb.security_id = mi.security_id;
        mt.security_id = mi.security_id;
        mc.security_id = mi.security_id;
    }
};

struct emessages : noncopyable
{
    exporter e;
    static const uint32_t pre_alloc = 150;
    message _;
    message ms[pre_alloc];
    uint32_t m_s;

    emessages(const std::string& push) : e(push), m_s()
    {
    }
    void ping(ttime_t etime, ttime_t time)
    {
        if(m_s != pre_alloc) {
            message_ping& p = ms[m_s++].mp;
            p.time = time;
            p.etime = etime;
            p.id = msg_ping;
        }
        send_messages();
    }
    void add_clean(uint32_t security_id, ttime_t etime, ttime_t time)
    {
        if(unlikely(m_s == pre_alloc))
            send_messages();
        
        message_clean& c = ms[m_s++].mc;
        c.time = time;
        c.etime = etime;
        c.id = msg_clean;
        c.security_id = security_id;
        c.source = 0;
    }
    void add_order(uint32_t security_id, int64_t level_id, price_t price, count_t count, ttime_t etime, ttime_t time)
    {
        if(unlikely(m_s == pre_alloc))
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
    void add_trade(uint32_t security_id, price_t price, count_t count, uint32_t direction, ttime_t etime, ttime_t time)
    {
        if(unlikely(m_s == pre_alloc))
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
    void send_messages()
    {
        if(m_s) {
            set_export_mtime(ms);
            e.proceed(ms, m_s);
            m_s = 0;
        }
    }
};

