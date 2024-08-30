/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../../evie/mfile.hpp"
#include "../../evie/config.hpp"
#include "../../evie/utils.hpp"

config::config(char_cit fname)
{
    auto cs = read_file(fname);
    str_holder smb = get_config_param<str_holder>(cs, "tickers");
    tickers = split_s(smb);
    trades = get_config_param<bool>(cs, "trades");
    orders = get_config_param<bool>(cs, "orders");
    if((!orders && !trades) || tickers.empty())
        throw str_exception("config::config() nothing to import");
    depth = get_config_param<uint32_t>(cs, "depth");
    
    push = get_config_param<str_holder>(cs, "push");
    exchange_id = get_config_param<str_holder>(cs, "exchange_id");
    feed_id = get_config_param<str_holder>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", orders: " << orders << ", depth: " << depth
        << ", exchange_id: " << exchange_id << ", feed_id: " << feed_id
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

