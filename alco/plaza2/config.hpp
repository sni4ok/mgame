/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/config.hpp"
#include "evie/vector.hpp"

struct config : stack_singleton<config>
{
    std::string cgate_host, app_name, key, local_pass;
    std::string cli_conn_recv;

    bool trades, orders;
    std::vector<std::string> tickers_filter;
    bool proceed_ticker(const std::string& ticker) const;

    std::string exchange_id, feed_id;
    std::string push;
    bool log_plaza;

    config(const char* fname);
};

