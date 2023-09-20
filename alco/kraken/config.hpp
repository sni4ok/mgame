/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/mstring.hpp"
#include "evie/singleton.hpp"

struct config : stack_singleton<config>
{
    mvector<mstring> tickers;
    bool trades, orders, bba;
    uint32_t depth; //orders depth 10, 25, 100, 500, 1000

    mstring exchange_id, feed_id;
    mstring push;
    bool log_lws;

    config(const char* fname);
};

