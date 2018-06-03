/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/config.hpp"

struct config : stack_singleton<config>
{
    std::string deep;
    std::string feed;
    std::vector<std::string> symbols;
    bool trades, book;
    std::string precision;
    std::string push;

    bool curl_log;
    std::string curl_logname; //optional, in case when set log all incoming messages from curl
    uint32_t sleep_time;

    config(const char* fname);
};

