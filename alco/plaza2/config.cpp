/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "evie/mfile.hpp"

#include <set>


config::config(const char* fname)
{
    auto buf = read_file(fname);
    cgate_host = get_config_param<std::string>(buf, "cgate_host");
    if(cgate_host == "127.0.0.1")
        cgate_host = "p2lrpcq://" + cgate_host;
    else
        cgate_host = "p2tcp://" + cgate_host;

    app_name = get_config_param<std::string>(buf, "app_name");
    key = get_config_param<std::string>(buf, "key");
    local_pass = get_config_param<std::string>(buf, "local_pass", true);
    if(!local_pass.empty())
        local_pass = ";local_pass=" + local_pass + ";";
    cli_conn_recv = cgate_host + ":4001;app_name=" + app_name + "_recv;name=noname_recv" + local_pass;
  
    trades = get_config_param<bool>(buf, "trades");
    orders = get_config_param<bool>(buf, "orders");

    std::string tfilter = get_config_param<std::string>(buf, "tickers_filter");
    if(tfilter != "*")
        tickers_filter = split(tfilter);

    log_plaza = get_config_param<bool>(buf, "log_plaza");
    exchange_id = get_config_param<std::string>(buf, "exchange_id");
    feed_id = get_config_param<std::string>(buf, "feed_id");
    push = get_config_param<std::string>(buf, "push");
}

bool config::proceed_ticker(const std::string& ticker) const
{
    if(tickers_filter.empty())
        return true;
    return std::find(tickers_filter.begin(), tickers_filter.end(), ticker) != tickers_filter.end();
}

