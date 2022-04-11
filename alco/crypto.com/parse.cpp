/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i: sec_id_by_name<lws_impl>
{
    config& cfg;

    lws_i() : cfg(config::instance())
    {
        std::stringstream sub;
        sub << "{\"method\":\"subscribe\",\"params\":{\"channels\":[";

        bool have_c = false;
        for(auto& v: cfg.tickers) {
            if(cfg.orders) {
                if(have_c)
                    sub << ",";
                sub << "\"book." << v << "." << cfg.depth << "\"";
                have_c = true;
            }
            if(cfg.trades) {
                if(have_c)
                    sub << ",";
                sub << "\"trade." << v << "\"";
                have_c = true;
            }
        }
        sub << "]},\"nonce\":" << (cur_mtime().value / (ttime_t::frac / 1000)) << "}";
        subscribes.push_back(sub.str());
    }

    void proceed(lws* wsi, void* in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder((const char*)in, len);
        iterator it = (iterator)in, ie = it + len;
        skip_fixed(it, "{");

        if(skip_if_fixed(it, "\"code\":0,\"method\":\"subscribe\",\"result\":{\"instrument_name\":\""))
        {
            iterator ne = std::find(it, ie, '\"');
            uint32_t security_id = get_security_id(it, ne, time);
            skip_fixed(ne, "\",\"subscription\":\"");
            if(skip_if_fixed(ne, "book"))
            {
                ttime_t etime;
                if(len > 40)
                {
                    it = ie - 40;
                    search_and_skip_fixed(it, ie, "\"t\":");
                    etime.value = lexical_cast<uint64_t>(it, std::find(it, ie, ',')) * (ttime_t::frac / 1000);
                }
                else
                    etime = ttime_t();

                add_clean(security_id, ttime_t(), time);
                it = std::find(ne, ie, '[') + 1;
                if(skip_if_fixed(it, "{\"bids\":["))
                {
                    for(;;)
                    {
                        if(skip_if_fixed(it, "["))
                        {
                            ne = std::find(it, ie, ',');
                            price_t price = read_price(it, ne);
                            it = ne + 1;
                            ne = std::find(it, ie, ',');
                            count_t count = read_count(it, ne);
                            add_order(security_id, price.value, price, count, etime, time);
                            it = std::find(ne, ie, ']');
                            ++it;
                            if(*it != ',')
                            {
                                if(*it == ']')
                                    ++it;
                                if(*it == ',')
                                    ++it;
                                break;
                            }
                            ++it;
                        }
                    }
                }
                if(skip_if_fixed(it, "\"asks\":["))
                {
                    for(;;)
                    {
                        if(skip_if_fixed(it, "["))
                        {
                            ne = std::find(it, ie, ',');
                            price_t price = read_price(it, ne);
                            it = ne + 1;
                            ne = std::find(it, ie, ',');
                            count_t count = read_count(it, ne);
                            count.value = -count.value;
                            add_order(security_id, price.value, price, count, etime, time);
                            it = std::find(ne, ie, ']');
                            ++it;
                            if(*it != ',')
                            {
                                if(*it == ']')
                                    ++it;
                                break;
                            }
                            ++it;
                        }
                    }
                }
                it = std::find(it, ie, ']');
                skip_fixed(it, "]}}");
                send_messages();
            }
            else if(skip_if_fixed(ne, "trade."))
            {
                it = std::find(ne, ie, '[') + 1;

                for(;;)
                {
                    skip_fixed(it, "{\"dataTime\":");
                    search_and_skip_fixed(it, ie, "\"s\":\"");
                    uint32_t direction;
                    if(skip_if_fixed(it, "BUY"))
                        direction = 1;
                    else {
                        skip_fixed(it, "SELL");
                        direction = 2;
                    }
                    skip_fixed(it, "\",\"p\":");
                    ne = std::find(it, ie, ',');
                    price_t price = read_price(it, ne);
                    it = ne + 1;
                    skip_fixed(it, "\"q\":");
                    ne = std::find(it, ie, ',');
                    count_t count = read_count(it, ne);
                    it = ne + 1;
                    skip_fixed(it, "\"t\":");
                    ne = std::find(it, ie, ',');
                    ttime_t etime;
                    etime.value = lexical_cast<uint64_t>(it, ne) * (ttime_t::frac / 1000);
                    it = std::find(ne + 1, ie, '}');
                    ++it;
                    if(it == ie || *it == ']')
                        break;
                    if(*it == ',')
                        ++it;
                    add_trade(security_id, price, count, direction, etime, time);
                }
                skip_fixed(it, "]}}");
                send_messages();
            }
            else
                throw std::runtime_error(es() % "unsupported table type in message: " % std::string((iterator)in, ie));
        }
        else if(skip_if_fixed(it, "\"id\":0,\"code\":0,\"method\":\"subscribe\"}"))
        {
        }
        else if(skip_if_fixed(it, "\"id\":"))
        {
            iterator ne = std::find(it, ie, ',');
            uint64_t id = lexical_cast<uint64_t>(it, ne);
            if(skip_if_fixed(ne, ",\"method\":\"public/heartbeat\"}"))
            {
                bs << "{\"id\":" << id << ",\"method\":\"public/respond-heartbeat\"}";
                send(wsi);
            }
            else
                throw std::runtime_error(es() % "unsupported message: " % std::string((iterator)in, ie));
        }
        else
            throw std::runtime_error(es() % "unsupported message: " % std::string(it, ie));
    }
};

void proceed_crypto_com(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
	lws_client_connect_info ccinfo = lws_client_connect_info();
	ccinfo.context = ls.context;
	ccinfo.address = "stream.crypto.com";
	ccinfo.port = 443;
    ccinfo.path = "/v2/market";
	
    ccinfo.userdata = (void*)&ls;
	ccinfo.protocol = "ws";
	ccinfo.origin = "origin";
	ccinfo.host = ccinfo.address;
	ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	lws_client_connect_via_info(&ccinfo);
}

