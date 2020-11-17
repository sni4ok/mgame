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
    bba = get_config_param<bool>(cs, "bba");
    if((!bba && !trades) || tickers.empty())
        throw std::runtime_error("config::config() nothing to import");
    
    push = get_config_param<std::string>(cs, "push");
    exchange_id = get_config_param<std::string>(cs, "exchange_id");
    feed_id = get_config_param<std::string>(cs, "feed_id");
    log_lws = get_config_param<bool>(cs, "log_lws");

    mlog() << "config() tickers: " << smb
        << ", trades: " << trades << ", bba: " << bba
        << ", exchange_id: " << exchange_id << ", feed_id: " << feed_id
        << ",\n    push: " << push << ", log_lws: " << log_lws;
}

