/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../alco.hpp"
#include "../lws.hpp"
#include "../utils.hpp"

#include "evie/fmap.hpp"

struct lws_i : lws_impl
{
    config& cfg;
    typedef my_basic_string<char, sizeof(message_instr::security) + 1> ticker;
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

    my_basic_string<char, 11> cur_date;
    uint64_t cur_date_time;
    ttime_t read_time(iterator& it)
    {
        if(unlikely(cur_date != str_holder(it, 10)))
        {
            if(*(it + 4) != '-' || *(it + 7) != '-')
                throw std::runtime_error(es() % "bad time: " % std::string(it, it + 26));
            struct tm t = tm();
            int y = my_cvt::atoi<int>(it, 4); 
            int m = my_cvt::atoi<int>(it + 5, 2); 
            int d = my_cvt::atoi<int>(it + 8, 2); 
            t.tm_year = y - 1900;
            t.tm_mon = m - 1;
            t.tm_mday = d;
            cur_date_time = timegm(&t) * my_cvt::p10<9>();
            cur_date = str_holder(it, 10);
            mlog() << "cur_date set " << cur_date;
        }
        it += 10;
        if(*it != 'T' || *(it + 3) != ':' || *(it + 6) != ':' || *(it + 9) != '.')
            throw std::runtime_error(es() % "bad time: " % std::string(it - 10, it + 16));
        uint64_t h = my_cvt::atoi<uint64_t>(it + 1, 2);
        uint64_t m = my_cvt::atoi<uint64_t>(it + 4, 2);
        uint64_t s = my_cvt::atoi<uint64_t>(it + 7, 2);
        uint64_t us = my_cvt::atoi<uint64_t>(it + 10, 6);
        it += 16;
        return ttime_t{cur_date_time + us * 1000 + (s + m * 60 + h * 3600) * my_cvt::p10<9>()};
    }
    void proceed(lws*, void* in, size_t len)
    {
        ttime_t time = get_cur_ttime();
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
            skip_fixed(it, "]],\"time\":\"");
            ttime_t etime = read_time(it);
            skip_fixed(it, "Z\"}");
            if(unlikely(it != ie))
                throw std::runtime_error(es() % "parsing message error: " % std::string((iterator)in, ie));

            s.proceed_book(e, p, c, etime, time);
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
            ttime_t etime = read_time(it);
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
	lws_client_connect_info ccinfo =lws_client_connect_info();
	ccinfo.context = ls.context;
	ccinfo.address = "ws-feed.pro.coinbase.com";
	ccinfo.port = 443;
    ccinfo.path = "/ws/2";
	
    ccinfo.userdata = (void*)&ls;
	ccinfo.protocol = "ws";
	ccinfo.origin = "origin";
	ccinfo.host = ccinfo.address;
	ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	lws_client_connect_via_info(&ccinfo);
}

