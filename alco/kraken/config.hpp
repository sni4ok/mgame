/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/config.hpp"

struct config : stack_singleton<config>
{
    std::vector<std::string> tickers;
    bool trades, orders, bba;
    uint32_t depth; //orders depth 10, 25, 100, 500, 1000

    std::string exchange_id, feed_id;
    std::string push;
    bool log_lws;

    config(const char* fname);
};

