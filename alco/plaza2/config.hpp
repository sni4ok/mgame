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

    std::vector<std::string> tickers_filter;
    bool proceed_ticker(const std::string& ticker) const;

    bool log_plaza;
   
    std::string exchange_id, feed_id;
    std::string push;
    config(const char* fname);
};

