/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i : sec_id_by_name<lws_impl>
{
    config& cfg;
    
    lws_i() : sec_id_by_name<lws_impl>(config::instance().push, config::instance().log_lws),
        cfg(config::instance())
    {
        mstream sub;
        sub << "{\"op\":\"subscribe\",\"args\":[";
        int i = 0;
        for(auto& v: cfg.tickers)
        {
            if(cfg.trades)
            {
                if(i++)
                    sub << ",";
                sub << "\"publicTrade." << v << "\"";
            }
            if(cfg.orders)
            {
                if(i++)
                    sub << ",";
                sub << "\"orderbook.1." << v << "\"";
            }
        }
        sub << "]}";
        subscribes.push_back(sub.str());
    }
    void proceed(lws*, char_cit in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);

        char_cit it = in, ie = it + len, ne;
        skip_fixed(it, "{\"");
        if(*it == 't')
        {
            skip_fixed(it, "topic\":\"");
            if(skip_if_fixed(it, "orderbook.1."))
            {
                ne = find(it, ie, '\"');
                uint32_t security_id = get_security_id(it, ne, time);
                it = ne + 1;
                skip_fixed(it, ",\"ts\":");
                ttime_t etime = milliseconds(cvt::atoi<int64_t>(it, 13));
                it += 13;
                skip_fixed(it, ",\"type\":\"");
                
                if(!skip_if_fixed(it, "snapshot\",\"data\":{\"s\":\""))
                    skip_fixed(it, "delta\",\"data\":{\"s\":\"");

                it = find(it, ie, '\"');
                skip_fixed(it, "\",\"b\":[");
                if(skip_if_fixed(it, "[\""))
                {
                    ne = find(it, ie, '\"');
                    price_t bp = lexical_cast<price_t>(it, ne);
                    it = ne + 1;
                    skip_fixed(it, ",\"");
                    ne = find(it, ie, '\"');
                    count_t bc = lexical_cast<count_t>(it, ne);
                    it = ne + 1;
                    skip_fixed(it, "]]");
                    add_order(security_id, 1, bp, bc, etime, time);
                }
                else
                    skip_fixed(it, "],\"u\":");

                skip_fixed(it, ",\"a\":[");
                if(skip_if_fixed(it, "[\""))
                {
                    ne = find(it, ie, '\"');
                    price_t ap = lexical_cast<price_t>(it, ne);
                    it = ne + 1;
                    skip_fixed(it, ",\"");
                    ne = find(it, ie, '\"');
                    count_t ac = lexical_cast<count_t>(it, ne);
                    ac.value = -ac.value;
                    it = ne + 1;
                    skip_fixed(it, "]],\"u\":");
                    add_order(security_id, 2, ap, ac, etime, time);
                }
                else
                    skip_fixed(it, "],\"u\":");
                
                if(ie - it > 50) [[unlikely]]
                    throw mexception(es() % "parsing message error: "
                        % str_holder(in, ie - in));
                
                send_messages();
            }
            else if(skip_if_fixed(it, "publicTrade."))
            {
                ne = find(it, ie, '\"');
                uint32_t security_id = get_security_id(it, ne, time);
                it = ne + 1;
                skip_fixed(it, ",\"ts\":");
                ttime_t etime = milliseconds(cvt::atoi<int64_t>(it, 13));
                it += 13;
                skip_fixed(it, ",\"type\":\"snapshot\",\"data\":[");
            rep:
                skip_fixed(it, "{\"i\":\"");
                search_and_skip_fixed(it, ie, "\"p\":\"");
                ne = find(it, ie, '\"');
                price_t p = lexical_cast<price_t>(it, ne);
                it = ne + 1;
                skip_fixed(it, ",\"v\":\"");
                ne = find(it, ie, '\"');
                count_t c = lexical_cast<count_t>(it, ne);
                it = ne + 1;
                skip_fixed(it, ",\"");
                int dir = 1;
                if(*it == 'S')
                    dir = 2;
                else if(*it != 'B') [[unlikely]]
                    throw mexception(es() % "unknown message type: "
                        % str_holder(in, ie - in));

                add_trade(security_id, p, c, dir, etime, time);
                it = find(it, ie, '{');
                if(it != ie)
                    goto rep;
                send_messages();
            }
            else [[unlikely]]
                throw mexception(es() % "unknown message type: " % str_holder(in, ie - in));
        }
        else if(!skip_if_fixed(it, "success"))
            throw mexception(es() % "unknown message type: " % str_holder(in, ie - in));
    }
};

void proceed_bybit(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
    lws_connect(ls, "stream.bybit.com", 443, "/v5/public/spot");
    //lws_connect(ls, "stream-testnet.bybit.com", 443, "/v5/public/spot");
}

