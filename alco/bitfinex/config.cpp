/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "evie/mfile.hpp"

config::config(const char* fname)
{
    std::string cs = read_file<std::string>(fname);
    deep = get_config_param<std::string>(cs, "deep", true);
    feed = "bitfinexcpp" + get_config_param<std::string>(cs, "feed");
    std::string smb = get_config_param<std::string>(cs, "symbols");
    symbols = split(smb);
    if(symbols.size() != 1)
        throw std::runtime_error("now 1 instrument per instance sometimes works");
    trades = get_config_param<bool>(cs, "trades");
    book = get_config_param<bool>(cs, "book");
    if((!book && !trades) || symbols.empty())
        throw std::runtime_error("config::config() nothing to import");
    precision = get_config_param<std::string>(cs, "precision");
    push = get_config_param<std::string>(cs, "push");
    curl_logname = get_config_param<std::string>(cs, "curl_log", true);
    curl_log = !curl_logname.empty();
    sleep_time = get_config_param<uint32_t>(cs, "sleep_time");

    mlog() << "conf() deep: " << deep << ", feed: " << feed <<  ", symbols: " << smb
        << ", trades: " << trades << ", book: " << book << ", precision: " << precision
        << ",\n    push: " << push << ", curl_log: " << curl_logname << ", sleep_time: " << sleep_time;
}

