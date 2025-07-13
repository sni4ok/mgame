/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i : lws_impl, read_time_impl
{
    config& cfg;
    typedef my_basic_string<sizeof(message_instr::security) + 1> ticker;
    fmap<ticker, security> securities;

    security& get_security(char_cit i, char_cit ie, ttime_t time)
    {
        ticker symbol(i, ie);
        auto it = securities.find(symbol);
        if(it == securities.end()) [[unlikely]] {
            security& s = securities[symbol];
            s.init(cfg.exchange_id, cfg.feed_id, mstring(i, ie));
            s.proceed_instr(e, time);
            return s;
        }
        else
            return it->second;
    }
    lws_i() : lws_impl(config::instance().push, config::instance().log_lws), cfg(config::instance())
    {
        mstring tickers = join_tickers(cfg.tickers);
        my_stream sub;
        sub << "{\"type\":\"subscribe\",\"product_ids\": [" << tickers << "],\"channels\": [";
        if(cfg.orders)
            sub << "\"level2\",";
        if(cfg.trades)
            sub << "\"matches\",";
        sub << "{\"name\": \"ticker\",\"product_ids\": [" << tickers << "]}]}";
        subscribes.push_back(sub.str());
    }
    void proceed(lws*, char_cit in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);
        char_cit it = in, ie = it + len;
        skip_fixed(it, "{\"type\":\"");
        if(skip_if_fixed(it, "l2update\",\"product_id\":\""))
        {
            char_cit ne = find(it, ie, '\"');
            security& s = get_security(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"changes\":[[\"");
        repeat:
            bool ask;
            if(skip_if_fixed(it, "sell\",\""))
                ask = true;
            else {
                skip_fixed(it, "buy\",\"");
                ask = false;
            }
            ne = find(it, ie, '\"');
            price_t p = lexical_cast<price_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"");
            ne = find(it, ie, '\"');
            count_t c = lexical_cast<count_t>(it, ne);
            if(ask)
                c.value = -c.value;
            it = ne + 1;
            skip_fixed(it, "]");
            s.proceed_book(e, p, c, ttime_t(), time);
            if(skip_if_fixed(it, ","))
            {
                skip_fixed(it, "[\"");
                goto repeat;
            }
            skip_fixed(it, "],\"time\":\"");
            ttime_t etime = read_time<6>(it);
            unused(etime);
            skip_fixed(it, "Z\"}");
            if(it != ie) [[unlikely]]
                throw mexception(es() % "parsing message error: " % str_holder(in, ie - in));
        }
        else if(skip_if_fixed(it, "match\",\"trade_id\":"))
        {
            it = find(it, ie, ',') + 1;
            skip_fixed(it, "\"maker_order_id\":\"");
            it = find(it, ie, '\"') + 1;
            skip_fixed(it, ",\"taker_order_id\":\"");
            it = find(it, ie, '\"') + 1;
            skip_fixed(it, ",\"side\":\"");
            int direction;
            if(skip_if_fixed(it, "sell\",\""))
                direction = 1;
            else {
                skip_fixed(it, "buy\",\"");
                direction = 2;
            }
            skip_fixed(it, "size\":\"");
            char_cit ne = find(it, ie, '\"');
            count_t c = lexical_cast<count_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"price\":\"");
            ne = find(it, ie, '\"');
            price_t p = lexical_cast<price_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"product_id\":\"");
            ne = find(it, ie, '\"');
            security& s = get_security(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"sequence\":");
            it = find(it, ie, ',') + 1;
            skip_fixed(it, "\"time\":\"");
            ttime_t etime = read_time<6>(it);
            skip_fixed(it, "Z\"}");
            if(it != ie) [[unlikely]]
                throw mexception(es() % "parsing message error: " % str_holder(in, ie - in));
        
            s.proceed_trade(e, direction, p, c, etime, time);
        }
        else if(skip_if_fixed(it, "heartbeat\""))
        {
            if(ie - it > 120)
                throw mexception(es() % "parsing message error: " % str_holder(in, ie - in));
        }
        else if(skip_if_fixed(it, "error\""))
            throw mexception(es() % "error message: " % str_holder(in, len));
    }
};

void proceed_coinbase(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
    lws_connect(ls, "ws-feed.pro.coinbase.com", 443, "/ws/2");
}

