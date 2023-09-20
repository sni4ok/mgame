/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "evie/mfile.hpp"
#include "evie/config.hpp"
#include "evie/utils.hpp"

config::config(const char* fname)
{
    auto cs = read_file(fname);
    mstring smb = get_config_param<mstring>(cs, "tickers");
    tickers = split(smb);
    trades = get_config_param<bool>(cs, "trades");
    
    snapshot = get_config_param<bool>(cs, "snapshot");
    step = get_config_param<mstring>(cs, "step");

    orders = get_config_param<bool>(cs, "orders");
    levels = get_config_param<mstring>(cs, "levels");

    bbo = get_config_param<bool>(cs, "bbo");

    if((!orders && !trades && !snapshot && !bbo) || tickers.empty())
        throw str_exception("config::config() nothing to import");

    if(int(snapshot) + int(orders) + int(bbo) > 1)
        throw str_exception("config::config() snapshot, orders and bbo mutually exclusive");

    push = get_config_param<mstring>(cs, "push");
    exchange_id = get_config_param<mstring>(cs, "exchange_id");
    feed_id = get_config_param<mstring>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", orders: " << orders << ", step: " << step
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

