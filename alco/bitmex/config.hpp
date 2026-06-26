/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../../evie/mstring.hpp"
#include "../../evie/singleton.hpp"

namespace bitmex
{

struct config : stack_singleton<config>
{
    mvector<mstring> tickers;
    bool trades, orders;

    mstring orders_table; //orderBookL2_25, orderBookL2, orderBook10

    mstring exchange_id, feed_id;
    mstring push;
    bool log_lws;

    config(char_cit fname);
};

}

