/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

ttime_t read_time(char_cit it, char_cit ie)
{
    char_cit ne = find(it, ie, '.');
    uint64_t time = my_cvt::atoi<uint32_t>(it, ne - it);
    it = ne + 1;
    uint64_t time_us = my_cvt::atoi<uint32_t>(it, 6);
    return ttime_t{time * ttime_t::frac + time_us * 1000};
}

struct lws_i : sec_id_by_name<lws_impl>
{
    config& cfg;
    
    struct impl;
    typedef void (lws_i::*func)(uint32_t security_id, ttime_t time, char_cit& it, char_cit ie);
    struct impl
    {
        uint32_t security_id;
        func f;
    };
    fmap<uint32_t, impl> parsers; //channel, impl

    lws_i() : sec_id_by_name<lws_impl>(config::instance().push, config::instance().log_lws), cfg(config::instance())
    {
        mstring s = "{\"event\":\"subscribe\",\"pair\":[" + join_tickers(cfg.tickers) + "],\"subscription\": {";
        if(cfg.orders) {
            my_stream sub;
            sub << s;
            if(cfg.depth)
                sub << "\"depth\":" << cfg.depth << ",";
            sub << "\"name\":\"book\"}}";
            subscribes.push_back(sub.str());
        }
        if(cfg.trades)
            subscribes.push_back(s + "\"name\":\"trade\"}}");
        if(cfg.bba)
            subscribes.push_back(s + "\"name\":\"spread\"}}");
    }
    
    price_t t_price;
    count_t t_count;
    ttime_t t_time;
    void parse_tick(char_cit& it, char_cit ie)
    {
        t_price = read_value(it, ie, read_price, false);
        t_count = read_value(it, ie, read_count, false);
        t_time = read_value(it, ie, read_time, true);
    }
    void parse_spread(uint32_t security_id, ttime_t time, char_cit& it, char_cit ie)
    {
        char_cit f = it;
        skip_fixed(it, "[");
        price_t bid_price = read_value(it, ie, read_price, false);
        price_t ask_price = read_value(it, ie, read_price, false);
        ttime_t etime = read_value(it, ie, read_time, false);
        count_t bid_count = read_value(it, ie, read_count, false);
        count_t ask_count = read_value(it, ie, read_count, true);
        ask_count.value = -ask_count.value;
        skip_fixed(it, "],\"spread\",\"");
        if(*(ie - 1) != ']' || ie - it > 20)
            throw mstring(es() % "parsing spread message error: " % str_holder(f, ie - f));
        it = ie;
        
        add_order(security_id, 1, bid_price, bid_count, etime, time);
        add_order(security_id, 2, ask_price, ask_count, etime, time);
       
        send_messages();
    }
    void parse_book(uint32_t security_id, ttime_t time, char_cit& it, char_cit ie)
    {
        bool snapshot = false;
        char_cit f = it;
        skip_fixed(it, "{");
        for(;;){
            skip_fixed(it, "\"");
            bool ask;
            if(*it == 'b')
                ask = false;
            else if(*it == 'a')
                ask = true;
            else throw mstring(es() % "parsing book message error: " % str_holder(f, ie - f));
            ++it;
            if(*it == 's') { //snapshot
                ++it;
                snapshot = true;
            }
            skip_fixed(it, "\":[");
            for(;;)
            {
                skip_fixed(it, "[");
                parse_tick(it, ie);
                if(ask)
                    t_count.value = -t_count.value;
                bool rep = false;
                if(*it == ',') {
                    skip_fixed(it, ",\"r\"");
                    rep = true;
                }
                add_order(security_id, t_price.value, t_price, t_count, (snapshot || rep) ? ttime_t() : t_time, time);
                skip_fixed(it, "]");
                if(*it == ',')
                    ++it;
                else if(*it == ']')
                    break;
            }
            ++it;
            if(*it == ',')
                ++it;
            else if(skip_if_fixed(it, "},\"book-"))
                break;
            else if(skip_if_fixed(it, "},{"))
                continue;
            else
                throw mstring(es() % "parsing book message error: " % str_holder(f, ie - f));
            if(skip_if_fixed(it, "\"c\":\""))
                break;
        }
        if(*(ie - 1) != ']' || ie - it > 35)
            throw mstring(es() % "parsing book message error: " % str_holder(f, ie - f));
        it = ie;
        send_messages();
    }
    void parse_trade(uint32_t security_id, ttime_t time, char_cit& it, char_cit ie)
    {
        char_cit f = it;
        skip_fixed(it, "[");
        for(;;)
        {
            skip_fixed(it, "[");
            parse_tick(it, ie);
            skip_fixed(it, ",\"");
            int dir;
            if(*it == 'b')
                dir = 1;
            else if(*it == 's')
                dir = 2;
            else throw mstring(es() % "unsupported trade direction, " % str_holder(f, ie - f));
            add_trade(security_id, t_price, t_count, dir, t_time, time);
            ++it;
            skip_fixed(it, "\",\"");
            ++it;//market or limit
            skip_fixed(it, "\",\"");
            //miscellaneous
            it = find(it, ie, '\"') + 1;
            skip_fixed(it, "]");
            if(*it == ',')
                ++it;
            else if(*it == ']')
                break;
        }
        ++it;
        skip_fixed(it, ",\"trade\",\"");
        if(*(ie - 1) != ']' || ie - it > 20)
            throw mstring(es() % "parsing trade message error: " % str_holder(f, ie - f));
        it = ie;
        send_messages();
    }
    void proceed(lws*, char_cit in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);
        char_cit it = in, ie = it + len;

        if(*it == '[')
        {
            ++it;
            char_cit ne = find(it, ie, ',');
            uint32_t channel = my_cvt::atoi<uint32_t>(it, ne - it);
            impl& i = parsers.at(channel);
            it = ne + 1;
            ((this)->*(i.f))(i.security_id, time, it, ie);
        }
        else if(skip_if_fixed(it, "{\"channelID\":"))
        {
            if(!cfg.log_lws)
                mlog() << "" << str_holder(it, ie - it);
            char_cit ne = find(it, ie, ',');
            uint32_t channel = my_cvt::atoi<uint32_t>(it, ne - it);
            it = ne + 1;
            skip_fixed(it, "\"channelName\":\"");
            search_and_skip_fixed(it, ie, "\"pair\":\"");
            ne = find(it, ie, '\"');
            uint32_t security_id = get_security_id(it, ne, time);
            it = ne + 1;
            search_and_skip_fixed(it, ie, "\"name\":\"");
            ne = find(it, ie, '\"');
            str_holder type(it, ne - it);
            it = ne + 1;
            skip_fixed(it, "}}");
            impl& i = parsers[channel];
            i.security_id = security_id;
            if(type == "book")
                i.f = &lws_i::parse_book;
            else if(type == "trade")
                i.f = &lws_i::parse_trade;
            else if(type == "spread")
                i.f = &lws_i::parse_spread;
            else
                throw mstring(es() % "unsupported type: " % str_holder(in, len));
        }
        else if(skip_if_fixed(it, "{\"connectionID\":"))
            it = ie;
        else if(skip_if_fixed(it, "{\"event\":\"heartbeat\"}"))
        {
        }
        if(it != ie) [[unlikely]]
        {
            mlog(mlog::critical) << "unsupported message: " << str_holder(in, len);
        }
    }
};

void proceed_kraken(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
    lws_connect(ls, "ws.kraken.com", 443, "/");
}

