/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../../evie/mfile.hpp"
#include "../../evie/config.hpp"
#include "../../evie/utils.hpp"

config::config(const char* fname)
{
    auto cs = read_file(fname);
    mstring smb = get_config_param<mstring>(cs, "tickers");
    tickers = split(smb);
    trades = get_config_param<bool>(cs, "trades");
    orders = get_config_param<bool>(cs, "orders");
    if((!orders && !trades) || tickers.empty())
        throw str_exception("config::config() nothing to import");
    
    precision = get_config_param<mstring>(cs, "precision");
    frequency = get_config_param<mstring>(cs, "frequency");
    length = get_config_param<mstring>(cs, "length");

    ping = get_config_param<uint32_t>(cs, "ping");

    push = get_config_param<mstring>(cs, "push");
    exchange_id = get_config_param<mstring>(cs, "exchange_id");
    feed_id = get_config_param<mstring>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", orders: " << orders << ", precision: " << precision
        << ", frequency: " << frequency << ", length: " << length << ", ping: " << ping 
        << ", exchange_id: " << exchange_id << ", feed_id: " << feed_id
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

