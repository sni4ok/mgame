/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../../evie/mstring.hpp"
#include "../../evie/singleton.hpp"

struct config : stack_singleton<config>
{
    mvector<mstring> tickers;
    bool trades, orders;
    u32 depth; //for book channel, Number of bids and asks to return. Allowed values: 10 or 150

    mstring exchange_id, feed_id;
    mstring push;
    bool log_lws;

    config(char_cit fname);
};

