/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

config::config(const char* fname)
{
    auto cs = read_file(fname);
    std::string smb = get_config_param<std::string>(cs, "tickers");
    tickers = split(smb);
    trades = get_config_param<bool>(cs, "trades");
    
    snapshot = get_config_param<bool>(cs, "snapshot");
    step = get_config_param<std::string>(cs, "step");

    orders = get_config_param<bool>(cs, "orders");
    levels = get_config_param<std::string>(cs, "levels");

    bbo = get_config_param<bool>(cs, "bbo");

    if((!orders && !trades && !snapshot && !bbo) || tickers.empty())
        throw std::runtime_error("config::config() nothing to import");

    if(int(snapshot) + int(orders) + int(bbo) > 1)
        throw std::runtime_error("config::config() snapshot, orders and bbo mutually exclusive");

    push = get_config_param<std::string>(cs, "push");
    exchange_id = get_config_param<std::string>(cs, "exchange_id");
    feed_id = get_config_param<std::string>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", orders: " << orders << ", step: " << step
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

