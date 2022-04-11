/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/config.hpp"

struct config : stack_singleton<config>
{
    std::vector<std::string> tickers;
    bool trades, orders;
    uint32_t depth; //for book channel, Number of bids and asks to return. Allowed values: 10 or 150

    std::string exchange_id, feed_id;
    std::string push;
    bool log_lws;

    config(const char* fname);
};

