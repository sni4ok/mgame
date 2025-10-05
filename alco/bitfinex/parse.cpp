/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i : sec_id_by_name<lws_impl>
{
    config& cfg;
    bool prec_R0;
    
    char subscribed[24];
    char trade[19];
    char book[17];
    char symbol[11];
    char event[9];

    struct impl;
    typedef void (lws_i::*func)(impl& i, ttime_t time, char_cit& it, char_cit ie);
    struct impl
    {
        u32 security_id;
        func f;
        u32 high;
        impl() : high()
        {
        }
    };
    const ttime_t ping;
    ttime_t ping_t;

    fmap<u32, impl> parsers; //channel, impl
    lws_i() : sec_id_by_name<lws_impl>(config::instance().push, config::instance().log_lws),
        cfg(config::instance()),
        subscribed("\"subscribed\",\"channel\":"),
        trade("\"trades\",\"chanId\":"),
        book("\"book\",\"chanId\":"),
        symbol("\"symbol\":\""),
        event("\"event\":"),
        ping(seconds(cfg.ping)), ping_t(cur_ttime())
    {
        prec_R0 = (cfg.precision == "R0");
        for(auto& v: cfg.tickers) {
            if(cfg.trades) {
                subscribes.push_back("{\"event\":\"subscribe\",\"channel\":\"trades\",\"symbol\":\"" + v + "\"}");
            }
            if(cfg.orders) {
                subscribes.push_back("{\"event\":\"subscribe\",\"channel\":\"book\",\"symbol\":\""
                    + v + "\",\"prec\":\"" + cfg.precision + "\",\"freq\":\"" + cfg.frequency + "\",\"len\":\"" + cfg.length + "\"}");
            }
        }
        send_messages();
    }
    void parse_trades(impl& i, ttime_t time, char_cit& it, char_cit ie)
    {
        if(*(it + 1) == '[')
        {
            //trade book after initialize
            it = ie - 3;
            skip_fixed(it, "]]]");
            return;
        }
        skip_fixed(it, "[");
        char_cit ne = find(it, ie, ',');
        u32 id = cvt::atoi<u32>(it, ne - it);
        ++ne;
        ttime_t etime = milliseconds(cvt::atoi<i64>(ne, 13));
        ne += 13;
        skip_fixed(ne, ",");
        it = find(ne, ie, ',');
        count_t count = lexical_cast<count_t>(ne, it);
        int dir = 1;
        if(count.value < 0)
        {
            dir = 2;
            count.value = -count.value;
        }
        skip_fixed(it, ",");
        ne = find(it, ie, ']');
        price_t price = lexical_cast<price_t>(it, ne);
        
        if(*ne == ']' && *(ne + 1) == ']')
            it = ne + 2;
        
        if(id > i.high)
        {
            add_trade(i.security_id, price, count, dir, etime, time);
            i.high = id;
            send_messages();
        }
    }
    void parse_orders(impl& i, ttime_t time, char_cit& it, char_cit ie)
    {
        bool one = *(it + 1) != '[';

        if(!one)
            skip_fixed(it, "[");
        char_cit ne;
        for(;;)
        {
            skip_fixed(it, "[");
            ne = find(it, ie, ',');
            if(prec_R0)
            {
                i64 level_id = cvt::atoi<i64>(it, ne - it);
                ++ne;
                it = find(ne, ie, ',');
                price_t price = lexical_cast<price_t>(ne, it);
                ++it;
                ne = find(it, ie, ']');
                count_t amount = lexical_cast<count_t>(it, ne);
                if(price != price_t())
                    add_order(i.security_id, level_id, price, amount, ttime_t(), time);
                else
                    add_order(i.security_id, level_id, price, count_t(), ttime_t(), time);
            }
            else
            {
                price_t price = lexical_cast<price_t>(it, ne);
                ++ne;
                it = find(ne, ie, ',');
                u32 count = cvt::atoi<u32>(ne, it - ne);
                ++it;
                ne = find(it, ie, ']');
                count_t amount = lexical_cast<count_t>(it, ne);
                if(count > 0)
                    add_order(i.security_id, price.value, price, amount, ttime_t(), time);
                else
                    add_order(i.security_id, price.value, price, count_t(), ttime_t(), time);
            }
            it = ne + 1;
            if(*it != ',')
                break;
            else
                ++it;
        }
        if(one)
            skip_fixed(it, "]");
        else
            skip_fixed(it, "]]");
        send_messages();
    }
    void add_channel(u32 channel, str_holder ticker, bool is_trades, ttime_t time)
    {
        mlog() << "add_channel: " << channel << ", ticker: " << ticker << ", is_trades: " << is_trades;
        impl& i = parsers[channel];
        i.security_id = get_security_id(ticker.begin(), ticker.end(), time);
        if(is_trades)
            i.f = &lws_i::parse_trades;
        else
            i.f = &lws_i::parse_orders;
    }
    void proceed(lws* wsi, char_cit in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);
        char_cit it = in, ie = it + len;

        if(*it == '[') [[likely]]
        {
            ++it;
            char_cit ne = find(it, ie, ',');
            u32 channel = cvt::atoi<u32>(it, ne - it);
            skip_fixed(ne, ",");
            ne = find(ne, ie, '[');

            if(ne != ie) [[likely]] {
                impl& i = parsers.at(channel);
                ((this)->*(i.f))(i, time, ne, ie);
                if(ne != ie)
                    throw mexception(es() % "parsing market message error: " % str_holder(in, len));
            }
            else {
                //possible "hb" message
                lws_impl::ping(ttime_t(), time);
            }
        }
        else if(*it == '{')
        {
            ++it;
            if(skip_if_fixed(it, event))
            {
                if(skip_if_fixed(it, subscribed))
                {
                    bool is_trades = false;
                    if(skip_if_fixed(it, trade))
                        is_trades = true;
                    else
                        skip_fixed(it, book);
                    char_cit ne = find(it, ie, ',');
                    u32 channel = cvt::atoi<u32>(it, ne - it);
                    ++ne;
                    skip_fixed(ne, symbol);
                    it = find(ne, ie, '\"');

                    str_holder ticker(ne, it - ne);
                    add_channel(channel, ticker, is_trades, time);
                    it = find(it, ie, '}') + 1;
                    if(it != ie)
                        throw mexception(es() % "parsing logic error message: " % str_holder(in, len));
                }
            }
            else
                mlog(mlog::critical) << "unsupported message: " << str_holder(it, len);
        }
        else
            mlog(mlog::critical) << "unsupported message: " << str_holder(it, len);

        if(!!ping) {
            if(time > ping_t + ping) {
                ping_t = time;
                bs << "{\"event\":\"ping\",\"cid\":\"" << ping_t.value / ttime_t::frac << "\"}";
                send(wsi);
            }
        }
    }
};

void proceed_bitfinex(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
    lws_connect(ls, "api-pub.bitfinex.com", 443, "/ws/2");
}

