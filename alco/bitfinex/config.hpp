/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/config.hpp"

struct config : stack_singleton<config>
{
    std::vector<std::string> tickers;
    bool trades, orders;

    std::string precision; //Level of price aggregation (P0, P1, P2, P3, P4).
    std::string frequency; //Frequency of updates (F0, F1). F0 = realtime / F1=2sec.
    std::string length;    //Number of price points ("25", "100")

    uint32_t ping; //bitfinex close connection without auto pings from websocket, this param set force pings
                   //in seconds, 0 for disable

    std::string exchange_id, feed_id;
    std::string push;
    bool log_lws;

    config(const char* fname);
};

