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

    security& get_security(iterator i, iterator ie, ttime_t time)
    {
        ticker symbol(i, ie);
        auto it = securities.find(symbol);
        if(unlikely(it == securities.end())) {
            security& s = securities[symbol];
            s.init(cfg.exchange_id, cfg.feed_id, std::string(i, ie));
            s.proceed_instr(e, time);
            return s;
        }
        else
            return it->second;
    }
    lws_i() : cfg(config::instance())
    {
        std::string tickers = join_tickers(cfg.tickers);
        std::stringstream sub;
        sub << "{\"type\":\"subscribe\",\"product_ids\": [" << tickers << "],\"channels\": [";
        if(cfg.orders)
            sub << "\"level2\",";
        if(cfg.trades)
            sub << "\"matches\",";
        sub << "{\"name\": \"ticker\",\"product_ids\": [" << tickers << "]}]}";
        subscribes.push_back(sub.str());
    }
    void proceed(lws*, void* in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder((const char*)in, len);
        iterator it = (iterator)in, ie = it + len;
        skip_fixed(it, "{\"type\":\"");
        if(skip_if_fixed(it, "l2update\",\"product_id\":\""))
        {
            iterator ne = std::find(it, ie, '\"');
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
            ne = std::find(it, ie, '\"');
            price_t p = read_price(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"");
            ne = std::find(it, ie, '\"');
            count_t c = read_count(it, ne);
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
            my_unused(etime);
            skip_fixed(it, "Z\"}");
            if(unlikely(it != ie))
                throw std::runtime_error(es() % "parsing message error: " % std::string((iterator)in, ie));
        }
        else if(skip_if_fixed(it, "match\",\"trade_id\":"))
        {
            it = std::find(it, ie, ',') + 1;
            skip_fixed(it, "\"maker_order_id\":\"");
            it = std::find(it, ie, '\"') + 1;
            skip_fixed(it, ",\"taker_order_id\":\"");
            it = std::find(it, ie, '\"') + 1;
            skip_fixed(it, ",\"side\":\"");
            int direction;
            if(skip_if_fixed(it, "sell\",\""))
                direction = 1;
            else {
                skip_fixed(it, "buy\",\"");
                direction = 2;
            }
            skip_fixed(it, "size\":\"");
            iterator ne = std::find(it, ie, '\"');
            count_t c = read_count(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"price\":\"");
            ne = std::find(it, ie, '\"');
            price_t p = read_price(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"product_id\":\"");
            ne = std::find(it, ie, '\"');
            security& s = get_security(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"sequence\":");
            it = std::find(it, ie, ',') + 1;
            skip_fixed(it, "\"time\":\"");
            ttime_t etime = read_time<6>(it);
            skip_fixed(it, "Z\"}");
            if(unlikely(it != ie))
                throw std::runtime_error(es() % "parsing message error: " % std::string((iterator)in, ie));
        
            s.proceed_trade(e, direction, p, c, etime, time);
        }
        else if(skip_if_fixed(it, "heartbeat\""))
        {
            if(ie - it > 120)
                throw std::runtime_error(es() % "parsing message error: " % std::string((iterator)in, ie));
        }
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

