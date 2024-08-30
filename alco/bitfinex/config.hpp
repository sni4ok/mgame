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

    mstring precision; //Level of price aggregation (R0, P0, P1, P2, P3, P4)
    mstring frequency; //Frequency of updates (F0, F1). F0 = realtime / F1=2sec
    mstring length;    //Number of price points ("25", "100")

    uint32_t ping; //bitfinex close connection without auto pings from websocket, this param set force pings
                   //in seconds, 0 for disable

    mstring exchange_id, feed_id;
    mstring push;
    bool log_lws;

    config(char_cit fname);
};

