/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../makoa/exports.hpp"

struct security
{
    message _;
    message_book mb;
    message_trade mt;
    message_clean mc;
    message_instr mi;
    message_ping mp;

    void proceed_book(exporter& e, price_t price, count_t count, ttime_t etime, ttime_t time);
    void proceed_trade(exporter& e, u32 direction, price_t price, count_t count, ttime_t etime, ttime_t time);
    void proceed_clean(exporter& e);
    void proceed_instr(exporter& e, ttime_t time);
    void proceed_ping(exporter& e, ttime_t etime);
    void init(const mstring& exchange_id, const mstring& feed_id, const mstring& ticker);
};

struct emessages
{
    exporter e;
    static const u32 pre_alloc = 150;
    message _;
    message ms[pre_alloc];
    u32 m_s;

    emessages(const mstring& push);
    emessages(const emessages&) = delete;
    void ping(ttime_t etime, ttime_t time);
    void add_clean(u32 security_id, ttime_t etime, ttime_t time);
    void add_order(u32 security_id, i64 level_id, price_t price, count_t count, ttime_t etime, ttime_t time);
    void add_trade(u32 security_id, price_t price, count_t count, u32 direction, ttime_t etime, ttime_t time);
    void send_messages();
};

