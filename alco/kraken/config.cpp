/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "evie/mfile.hpp"

config::config(const char* fname)
{
    std::string cs = read_file<std::string>(fname);
    std::string smb = get_config_param<std::string>(cs, "tickers");
    tickers = split(smb);
    trades = get_config_param<bool>(cs, "trades");
    orders = get_config_param<bool>(cs, "orders");
    depth = get_config_param<uint32_t>(cs, "depth", true);
    bba = get_config_param<bool>(cs, "bba");

    if((!orders && !trades && !bba) || tickers.empty())
        throw std::runtime_error("config::config() nothing to import");
    
    if(orders && bba)
        throw std::runtime_error("config::config() orders and bbo mutually exclusive");

    push = get_config_param<std::string>(cs, "push");
    exchange_id = get_config_param<std::string>(cs, "exchange_id");
    feed_id = get_config_param<std::string>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", orders: " << orders << ", bba: " << bba
        << ", exchange_id: " << exchange_id << ", feed_id: " << feed_id
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

