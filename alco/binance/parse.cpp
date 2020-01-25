/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../alco.hpp"
#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i : sec_id_by_name<lws_impl>
{
    config& cfg;
    
    lws_i() : cfg(config::instance())
    {
        std::stringstream sub;
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
    void proceed(lws* wsi, void* in, size_t len)
    {
        ttime_t time = get_cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder((const char*)in, len);
        iterator it = (iterator)in, ie = it + len;
        skip_fixed(it, "{\"");
        if(*it == 'u')
        {
            it = std::find(it + 1, ie, ',');
            skip_fixed(it, ",\"s\":\"");
            iterator ne = std::find(it, ie, '\"');
            uint32_t security_id = get_security_id(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"b\":\"");
            ne = std::find(it, ie, '\"');
            price_t bp = read_price(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"B\":\"");
            ne = std::find(it, ie, '\"');
            count_t bc = read_count(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"a\":\"");
            ne = std::find(it, ie, '\"');
            price_t ap = read_price(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"A\":\"");
            ne = std::find(it, ie, '\"');
            count_t ac = read_count(it, ne);
            ac.value = -ac.value;
            it = ne + 1;
            skip_fixed(it, "}");
            
            if(unlikely(it != ie))
                throw std::runtime_error(es() % "parsing message error: " % std::string((iterator)in, ie));
            
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
            iterator ne = std::find(it, ie, '\"');
            uint32_t security_id = get_security_id(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"t\":");
            it = std::find(it, ie, ',');
            ++it;
            skip_fixed(it, "\"p\":\"");
            ne = std::find(it, ie, '\"');
            price_t p = read_price(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"q\":\"");
            ne = std::find(it, ie, '\"');
            count_t c = read_count(it, ne);
            it = ne + 1;
            skip_fixed(it, ",\"b\":");
            ne = std::find(it, ie, 'T');
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
                throw std::runtime_error(es() % "parsing message error: " % std::string((iterator)in, ie));
            ne = std::find(it, ie, '}');
            if(((ne - it) != 4 && (ne - it) != 5) || (ne + 1) != ie)
                throw std::runtime_error(es() % "parsing message error: " % std::string((iterator)in, ie));
            add_trade(security_id, p, c, direction, etime, time);
            send_messages();
        }
        else if(!skip_if_fixed(it, "result"))
            throw std::runtime_error(es() % "unknown message type: " % std::string((iterator)in, ie));
    }
};

void proceed_binance(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
	lws_client_connect_info ccinfo =lws_client_connect_info();
	ccinfo.context = ls.context;
	ccinfo.address = "stream.binance.com";
	ccinfo.port = 9443;
    ccinfo.path = "/ws";
	
    ccinfo.userdata = (void*)&ls;
	ccinfo.protocol = "ws";
	ccinfo.origin = "origin";
	ccinfo.host = ccinfo.address;
	ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	lws_client_connect_via_info(&ccinfo);
}

