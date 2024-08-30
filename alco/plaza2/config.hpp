/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../../evie/singleton.hpp"
#include "../../evie/mstring.hpp"

struct config : stack_singleton<config>
{
    mstring cgate_host, app_name, key, local_pass;
    mstring cli_conn_recv;

    bool trades, orders;
    mvector<mstring> tickers_filter;
    bool proceed_ticker(const mstring& ticker) const;

    mstring exchange_id, feed_id;
    mstring push;
    bool log_plaza;

    config(char_cit fname);
};

