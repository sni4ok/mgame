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
    bba = get_config_param<bool>(cs, "bba");
    if((!bba && !trades) || tickers.empty())
        throw str_exception("config::config() nothing to import");
    
    push = get_config_param<mstring>(cs, "push");
    exchange_id = get_config_param<mstring>(cs, "exchange_id");
    feed_id = get_config_param<mstring>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", bba: " << bba
        << ", exchange_id: " << exchange_id << ", feed_id: " << feed_id
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

