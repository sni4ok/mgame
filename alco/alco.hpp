/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../makoa/exports.hpp"

struct emessages
{
    exporter e;
    static const u32 pre_alloc = 150;
    message ms[pre_alloc];
    u32 m_s;

    emessages(const mstring& push);
    emessages(const emessages&) = delete;
    void ping(ttime_t etime, ttime_t time);
    void add_clean(u32 security_id, ttime_t etime, ttime_t time);
    void add_order(u32 security_id, i64 level_id, price_t price, count_t count, ttime_t etime, ttime_t time);
    void add_trade(u32 security_id, price_t price, count_t count, u32 direction, ttime_t etime, ttime_t time);
    void send_messages();
    u32 proceed_instr(str_holder exchange_id, str_holder feed_id, str_holder ticker, ttime_t time);
};

