/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/types.hpp"
#include "tyra/tyra.hpp"
#include "evie/fmap.hpp"

struct security
{
    tyra_msg<message_book> mb;
    tyra_msg<message_trade> mt;
    tyra_msg<message_clean> mc;
    tyra_msg<message_instr> mi;

    void proceed_book(tyra& t, price_t price, count_t count, ttime_t etime)
    {
        mb.price = price;
        mb.count = count;
        mb.etime = etime;
        mb.time = get_cur_ttime();
        t.send(mb);
    }
    void proceed_trade(tyra& t, uint32_t direction, price_t price, count_t count, ttime_t etime)
    {
        mt.direction = direction;
        mt.price = price;
        mt.count = count;
        mt.etime = etime;
        mt.time = get_cur_ttime();
        t.send(mt);
    }
    void proceed_clean(tyra& t)
    {
        mc.time = get_cur_ttime();
        t.send(mc);
    }
    void init(const std::string& exchange_id, const std::string& feed_id, const std::string& ticker)
    {
        mb = tyra_msg<message_book>();
        mt = tyra_msg<message_trade>();
        mc = tyra_msg<message_clean>();
        mi = tyra_msg<message_instr>();

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

        crc32 crc(0);
        crc.process_bytes((const char*)(&(static_cast<message_instr&>(mi))), sizeof(message_instr) - 12);
        mi.security_id = crc.checksum();
        mi.time = get_cur_ttime();

        mb.security_id = mi.security_id;
        mt.security_id = mi.security_id;
        mc.security_id = mi.security_id;
    }
};

