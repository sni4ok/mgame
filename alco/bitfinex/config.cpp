/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../../evie/mfile.hpp"
#include "../../evie/config.hpp"
#include "../../evie/utils.hpp"
#include "../../evie/mlog.hpp"

config::config(char_cit fname)
{
    auto cs = read_file(fname);
    str_holder smb = get_config_param<str_holder>(cs, "tickers");
    tickers = split_s(smb);
    trades = get_config_param<bool>(cs, "trades");
    orders = get_config_param<bool>(cs, "orders");
    if((!orders && !trades) || tickers.empty())
        throw str_exception("config::config() nothing to import");
    
    precision = get_config_param<str_holder>(cs, "precision");
    frequency = get_config_param<str_holder>(cs, "frequency");
    length = get_config_param<str_holder>(cs, "length");

    ping = get_config_param<uint32_t>(cs, "ping");

    push = get_config_param<str_holder>(cs, "push");
    exchange_id = get_config_param<str_holder>(cs, "exchange_id");
    feed_id = get_config_param<str_holder>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", orders: " << orders << ", precision: " << precision
        << ", frequency: " << frequency << ", length: " << length << ", ping: " << ping 
        << ", exchange_id: " << exchange_id << ", feed_id: " << feed_id
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

