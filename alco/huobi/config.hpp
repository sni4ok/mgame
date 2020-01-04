/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/config.hpp"

struct config : stack_singleton<config>
{
    std::vector<std::string> tickers;
    bool trades, orders, snapshot, bbo;
    std::string step; //for snapshot, possible values: step0, step1, step2, step3, step4, step5
    std::string levels; //for orders, possible values: 150
    
    std::string ssl_ca_file; //https://github.com/HuobiRDCenter/huobi_Cpp/tree/master/cert/cert.pem

    std::string exchange_id, feed_id;
    std::string push;
    bool log_lws;

    config(const char* fname);
};

