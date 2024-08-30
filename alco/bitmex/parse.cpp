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
    ttime_t etime;

    lws_i() : sec_id_by_name<lws_impl>(config::instance().push, config::instance().log_lws), cfg(config::instance()), orders_table(cfg.orders_table.c_str(),
        cfg.orders_table.size()), trades_table("trade"), etime()
    {
        str_holder sb("{\"op\":\"subscribe\",\"args\":\"");
        str_holder se("\"}");

        for(auto& v: cfg.tickers) {
            //subscribes.push_back(sb + "instrument:" + v + se);
            if(cfg.orders)
                subscribes.push_back(sb + cfg.orders_table + ":" + v + se);
            if(cfg.trades)
                subscribes.push_back(sb + mstring("trade") + ":" + v + se);
        }
    }
    void parse_ticks(char_cit& it, char_cit ie, uint32_t security_id, ttime_t time, bool ask)
    {
        if(this->m_s == 0)
            add_clean(security_id, ttime_t(), time);
        for(;;)
        {
            skip_fixed(it, "[");
            char_cit ne = find(it, ie, ',');
            price_t p = read_price(it, ne);
            it = ne + 1;
            ne = find(it, ie, ']');
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

    void proceed(lws*, char_cit in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);
        char_cit it = in, ie = it + len;

        skip_fixed(it, "{\"");
        if(skip_if_fixed(it, "table\":\""))
        {
            char_cit ne = find(it, ie, '\"');
            str_holder table(it, ne - it);
            it = ne;
            if(table == orders_table)
            {
                search_and_skip_fixed(it, ie, "data\":[");
                skip_fixed(it, "{");
                for(;;)
                {
                    skip_fixed(it, "\"symbol\":\"");
                    ne = find(it, ie, '\"');
                    uint32_t security_id = get_security_id(it, ne, time);
                    it = ne + 1;
                    skip_fixed(it, ",\"");

                    if(skip_if_fixed(it, "id\":"))
                    {
                        ne = find(it, ie, ',');
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
                            ne = find_if(it, ie, [](char c) {return c == '}' || c == ',';});
                            c = read_count(it, ne);
                            if(ask)
                                c.value = -c.value;
                            it = ne;
                        }
                        price_t p = price_t();
                        if(skip_if_fixed(it, ",\"price\":"))
                        {
                            ne = find(it, ie, ',');
                            p = read_price(it, ne);
                            it = ne;
                        }
                        skip_fixed(it, ",\"timestamp\":\"");
                        {
                            etime = read_time<3>(it);
                            skip_fixed(it, "Z\"");
                        }
                        it = find(ne, ie, '}');
                        skip_fixed(it, "}");
                        add_order(security_id, id, p, c, etime, time);
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
                                if(it != ie) [[unlikely]]
                                    throw mexception(es() % "parsing message error: " % str_holder(in, len));
                                return;
                            }
                        }
                    }
                }
                send_messages();
                if(it != ie) [[unlikely]]
                    throw mexception(es() % "parsing message error: " % str_holder(in, len));
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
                    etime = read_time<3>(it);
                    skip_fixed(it, "Z\",\"symbol\":\"");
                    ne = find(it, ie, '\"');
                    uint32_t security_id = get_security_id(it, ne, time);
                    it = ne + 1;
                    str_holder side = read_named_value(",\"side\":\"", it, ie, '\"', read_str);
                    int direction;
                    if(side == str_holder("Buy"))
                        direction = 1;
                    else if(side == str_holder("Sell"))
                        direction = 2;
                    else
                        throw mexception(es() % "unknown trade direction in message: " % str_holder(in, len));
                    count_t c = read_named_value(",\"size\":", it, ie, ',', read_count);
                    price_t p = read_named_value("\"price\":", it, ie, ',', read_price);
                    add_trade(security_id, p, c, direction, etime, time);
                    it = find(it, ie, '}');
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
            else [[unlikely]]
            {
                throw mexception(es() % "unknown table name: " % str_holder(in, len));
            }
            if(it != ie) [[unlikely]]
                throw mexception(es() % "parsing message error: " % str_holder(in, len));
        }
        else if(skip_if_fixed(it, "success\""))
        {
            mlog(mlog::info) << str_holder(in, len);
        }
        else if(skip_if_fixed(it, "info\""))
        {
            mlog(mlog::info) << str_holder(in, len);
        }
        else
        {
            mlog(mlog::critical) << "unsupported message: " << str_holder(in, len);
        }
    }
};

void proceed_bitmex(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
    lws_connect(ls, "www.bitmex.com", 443, "/realtime");
}

