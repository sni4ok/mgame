/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../../evie/mstring.hpp"
#include "../../evie/singleton.hpp"

struct config : stack_singleton<config>
{
    mvector<mstring> tickers;
    bool trades, bba;

    mstring exchange_id, feed_id;
    mstring push;
    bool log_lws;

    config(char_cit fname);
};

