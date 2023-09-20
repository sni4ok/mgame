/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/mstring.hpp"
#include "evie/singleton.hpp"

struct config : stack_singleton<config>
{
    mvector<mstring> tickers;
    bool trades, orders, snapshot, bbo;
    mstring step; //for snapshot, possible values: step0, step1, step2, step3, step4, step5
    mstring levels; //for orders, possible values: 150
    
    mstring exchange_id, feed_id;
    mstring push;
    bool log_lws;

    config(const char* fname);
};

