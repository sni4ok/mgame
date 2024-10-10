/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i : sec_id_by_name<lws_impl>
{
    config& cfg;
    
    lws_i() : sec_id_by_name<lws_impl>(config::instance().push, config::instance().log_lws), cfg(config::instance())
    {
        my_stream sub;
        sub << "{\"method\":\"SUBSCRIBE\",\"params\":[";
        int i = 0;
        for(auto& v: cfg.tickers) {
            if(cfg.trades) {
                if(i++)
                    sub << ",";
                sub << "\"" << v << "@trade\"";
            }
            if(cfg.bba) {
                if(i++)
                    sub << ",";
                sub << "\"" << v << "@bookTicker\"";
            }
        }
        sub << "],\"id\":1}";
        subscribes.push_back(sub.str());
    }
    void proceed(lws*, char_cit in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);
        char_cit it = in, ie = it + len;
        skip_fixed(it, "{\"");
        if(*it == 'u')
        {
            it = find(it + 1, ie, ',');
            skip_fixed(it, ",\"s\":\"");
            char_cit ne = find(it, ie, '\"');
            uint32_t security_id = get_security_id(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"b\":\"");
            ne = find(it, ie, '\"');
            price_t bp = lexical_cast<price_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"B\":\"");
            ne = find(it, ie, '\"');
            count_t bc = lexical_cast<count_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"a\":\"");
            ne = find(it, ie, '\"');
            price_t ap = lexical_cast<price_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"A\":\"");
            ne = find(it, ie, '\"');
            count_t ac = lexical_cast<count_t>(it, ne);
            ac.value = -ac.value;
            it = ne + 1;
            skip_fixed(it, "}");
            
            if(it != ie) [[unlikely]]
                throw mexception(es() % "parsing message error: " % str_holder(in, ie - in));
            
            add_order(security_id, 2, ap, ac, ttime_t(), time);
            add_order(security_id, 1, bp, bc, ttime_t(), time);
            send_messages();
        }
        else if(*it == 'e')
        {
            ++it;
            skip_fixed(it, "\":\"trade\",\"E\":");
            //event time
            //ttime_t etime = {my_cvt::atoi<uint64_t>(it, 13) * (ttime_t::frac / 1000)};
            it = it + 14;
            skip_fixed(it, "\"s\":\"");
            char_cit ne = find(it, ie, '\"');
            uint32_t security_id = get_security_id(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"t\":");
            it = find(it, ie, ',');
            ++it;
            skip_fixed(it, "\"p\":\"");
            ne = find(it, ie, '\"');
            price_t p = lexical_cast<price_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"q\":\"");
            ne = find(it, ie, '\"');
            count_t c = lexical_cast<count_t>(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"b\":");
            ne = find(it, ie, 'T');
            it = ne + 1;
            skip_fixed(it, "\":");
            ttime_t etime = {my_cvt::atoi<uint64_t>(it, 13) * (ttime_t::frac / 1000)};
            it = it + 14;
            skip_fixed(it, "\"m\":");
            int direction;
            if(*it == 'f') {
                ++it;
                skip_fixed(it, "alse,\"M\":");
                direction = 1;
            }
            else if(*it == 't') {
                ++it;
                skip_fixed(it, "rue,\"M\":");
                direction = 2;
            }
            else 
                throw mexception(es() % "parsing message error: " % str_holder(in, ie - in));
            ne = find(it, ie, '}');
            if(((ne - it) != 4 && (ne - it) != 5) || (ne + 1) != ie)
                throw mexception(es() % "parsing message error: " % str_holder(in, ie - in));
            add_trade(security_id, p, c, direction, etime, time);
            send_messages();
        }
        else if(!skip_if_fixed(it, "result"))
            throw mexception(es() % "unknown message type: " % str_holder(in, ie - in));
    }
};

void proceed_binance(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
    lws_connect(ls, "stream.binance.com", 9443, "/ws");
}

