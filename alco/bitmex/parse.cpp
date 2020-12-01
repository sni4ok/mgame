/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i : sec_id_by_name<lws_impl>, read_time_impl
{
    config& cfg;

    str_holder orders_table, trades_table;

    lws_i() : cfg(config::instance()), orders_table(cfg.orders_table.c_str(), cfg.orders_table.size()),
    trades_table("trade")
    {
        std::string sb = "{\"op\":\"subscribe\",\"args\":\"";
        std::string se = "\"}";

        for(auto& v: cfg.tickers) {
            if(cfg.orders)
                subscribes.push_back(sb + cfg.orders_table + ":" + v + se);
            if(cfg.trades)
                subscribes.push_back(sb + "trade" + ":" + v + se);
        }
    }
    void parse_ticks(iterator& it, iterator ie, uint32_t security_id, ttime_t time, bool ask)
    {
        if(this->m_s == 0)
            add_clean(security_id, ttime_t(), time);
        for(;;)
        {
            skip_fixed(it, "[");
            iterator ne = std::find(it, ie, ',');
            price_t p = read_price(it, ne);
            it = ne + 1;
            ne = std::find(it, ie, ']');
            count_t c = read_count(it, ne);
            if(ask)
                c.value = -c.value;
            it = ne;
            skip_fixed(it, "]");
            add_order(security_id, p.value, p, c, ttime_t(), time);
            if(*it == ',')
                ++it;
            else if(*it == ']')
                break;
        }
        skip_fixed(it, "]");
    }

    ttime_t etime = ttime_t();
    void proceed(lws* wsi, void* in, size_t len)
    {
        ttime_t time = get_cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder((const char*)in, len);
        iterator it = (iterator)in, ie = it + len;

        skip_fixed(it, "{\"");
        if(skip_if_fixed(it, "table\":\""))
        {
            iterator ne = std::find(it, ie, '\"');
            str_holder table(it, ne - it);
            it = ne;
            if(table == orders_table)
            {
                search_and_skip_fixed(it, ie, "data\":[");
                skip_fixed(it, "{");
                for(;;)
                {
                    skip_fixed(it, "\"symbol\":\"");
                    ne = std::find(it, ie, '\"');
                    uint32_t security_id = get_security_id(it, ne, time);
                    it = ne + 1;
                    skip_fixed(it, ",\"");

                    if(skip_if_fixed(it, "id\":"))
                    {
                        ne = std::find(it, ie, ',');
                        uint64_t id  = my_cvt::atoi<uint64_t>(it, ne - it);
                        it = ne + 1;
                        skip_fixed(it, "\"side\":\"");
                        bool ask;
                        if(skip_if_fixed(it, "Sell"))
                            ask = true;
                        else {
                            skip_fixed(it, "Buy");
                            ask = false;
                        }
                        skip_fixed(it, "\"");
                        count_t c = count_t();
                        if(skip_if_fixed(it, ",\"size\":")) {
                            ne = std::find_if(it, ie, [](char c) {return c == '}' || c == ',';});
                            c = read_count(it, ne);
                            if(ask)
                                c.value = -c.value;
                            it = ne;
                        }
                        price_t p = price_t();
                        if(*it == ',') {
                            it = ne + 1;
                            skip_fixed(it, "\"price\":");
                            ne = std::find(it, ie, '}');
                            p = read_price(it, ne);
                            it = ne;
                        }
                        skip_fixed(it, "}");
                        add_order(security_id, id, p, c, ttime_t(), time);
                        if(skip_if_fixed(it, "]}"))
                            break;
                        skip_fixed(it, ",{");
                    }
                    else {
                        for(;;)
                        {
                            if(skip_if_fixed(it, "bids\":["))
                            {
                                parse_ticks(it, ie, security_id, time, false);
                            }
                            else if(skip_if_fixed(it, "asks\":["))
                            {
                                parse_ticks(it, ie, security_id, time, true);
                            }
                            else if(skip_if_fixed(it, ",\""))
                            {
                            }
                            else if(skip_if_fixed(it, "timestamp\":\""))
                            {
                                etime = read_time<3>(it);
                                skip_fixed(it, "Z\"");
                            }
                            else
                            {
                                skip_fixed(it, "}]}");
                                if(etime.value) {
                                    for(message *f = this->ms, *t = this->ms + m_s; f != t; ++f)
                                        f->t.etime = etime;
                                    etime = ttime_t();
                                }
                                send_messages();
                                if(unlikely(it != ie))
                                    throw std::runtime_error(es() % "parsing message error: " % str_holder((iterator)in, len));
                                return;
                            }
                        }
                    }
                }
                send_messages();
                if(unlikely(it != ie))
                    throw std::runtime_error(es() % "parsing message error: " % str_holder((iterator)in, len));
            }
            else if(table == trades_table)
            {
                str_holder action = read_named_value("\",\"action\":\"", it, ie, '\"', read_str);
                if(action == str_holder("partial")) //partial table contains old trades
                    return;

                search_and_skip_fixed(it, ie, "data\":[");

                for(;;)
                {
                    skip_fixed(it, "{\"timestamp\":\"");
                    ttime_t etime = read_time<3>(it);
                    skip_fixed(it, "Z\",\"symbol\":\"");
                    ne = std::find(it, ie, '\"');
                    uint32_t security_id = get_security_id(it, ne, time);
                    it = ne + 1;
                    str_holder side = read_named_value(",\"side\":\"", it, ie, '\"', read_str);
                    int direction;
                    if(side == str_holder("Buy"))
                        direction = 1;
                    else if(side == str_holder("Sell"))
                        direction = 2;
                    else
                        throw std::runtime_error(es() % "unknown trade direction in message: " % str_holder((iterator)in, len));
                    count_t c = read_named_value(",\"size\":", it, ie, ',', read_count);
                    price_t p = read_named_value("\"price\":", it, ie, ',', read_price);
                    add_trade(security_id, p, c, direction, etime, time);
                    it = std::find(it, ie, '}');
                    skip_fixed(it, "}");
                    if(*it == ',') {
                        ++it;
                        continue;
                    }
                    else
                        break;
                }
                skip_fixed(it, "]}");
                send_messages();
            }
            else
            {
                throw std::runtime_error(es() % "unknown table name: " % str_holder((iterator)in, len));
            }
            if(unlikely(it != ie))
                throw std::runtime_error(es() % "parsing message error: " % str_holder((iterator)in, len));
        }
        else if(skip_if_fixed(it, "success\""))
        {
            mlog(mlog::info) << str_holder((iterator)in, len);
        }
        else if(skip_if_fixed(it, "info\""))
        {
            mlog(mlog::info) << str_holder((iterator)in, len);
        }
        else
        {
            mlog(mlog::critical) << "unsupported message: " << str_holder((iterator)in, len);
        }
    }
};

void proceed_bitmex(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
	lws_client_connect_info ccinfo = lws_client_connect_info();
	ccinfo.context = ls.context;
    //wss://www.bitmex.com/realtime
	ccinfo.address = "www.bitmex.com";
	ccinfo.port = 443;
    ccinfo.path = "/realtime";
	
    ccinfo.userdata = (void*)&ls;
	ccinfo.protocol = "ws";
	ccinfo.origin = "origin";
	ccinfo.host = ccinfo.address;
	ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	lws_client_connect_via_info(&ccinfo);
}

