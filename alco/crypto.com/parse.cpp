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

    void proceed(lws* wsi, const char* in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);
        iterator it = in, ie = it + len;
        skip_fixed(it, "{\"id\":");
        iterator ne = it;
        it = std::find(it, ie, ',');
        if(skip_if_fixed(it, ",\"code\":0,\"method\":\"subscribe\",\"result\":{\"channel\":"))
        {
            bool book = false;
            if(skip_if_fixed(it, "\"book\",\"subscription\":\"book."))
            {
                ne = std::find(it, ie, '.');
                book = true;
            }
            else
            {
                skip_fixed(it, "\"trade\",\"subscription\":\"trade.");
                ne = std::find(it, ie, '\"');
            }

            uint32_t security_id = get_security_id(it, ne, time);
            search_and_skip_fixed(it, ie, "\"data\":[");
            if(book)
            {
                ttime_t etime;
                if(len > 80)
                {
                    ne = ie - 80;
                    search_and_skip_fixed(ne, ie, "\"t\":");
                    etime.value = lexical_cast<uint64_t>(ne, std::find(ne, ie, ',')) * (ttime_t::frac / 1000);
                }
                else
                    etime = ttime_t();

                add_clean(security_id, ttime_t(), time);
                if(skip_if_fixed(it, "{\"asks\":["))
                {
                    for(;;)
                    {
                        if(skip_if_fixed(it, "["))
                        {
                            skip_fixed(it, "\"");
                            ne = std::find(it, ie, '"');
                            price_t price = read_price(it, ne);
                            it = ne + 2;
                            skip_fixed(it, "\"");
                            ne = std::find(it, ie, '"');
                            count_t count = read_count(it, ne);
                            count.value = -count.value;
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
                if(skip_if_fixed(it, "\"bids\":["))
                {
                    for(;;)
                    {
                        if(skip_if_fixed(it, "["))
                        {
                            skip_fixed(it, "\"");
                            ne = std::find(it, ie, '"');
                            price_t price = read_price(it, ne);
                            it = ne + 2;
                            skip_fixed(it, "\"");
                            ne = std::find(it, ie, '"');
                            count_t count = read_count(it, ne);
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
            else
            {
                for(;;)
                {
                    skip_fixed(it, "{\"d\":\"");
                    search_and_skip_fixed(it, ie, "\",\"t\":");
                    ne = std::find(it, ie, ',');
                    ttime_t etime;
                    etime.value = lexical_cast<uint64_t>(it, ne) * (ttime_t::frac / 1000);
                    skip_fixed(ne, ",\"p\":\"");
                    it = std::find(ne, ie, '\"');
                    price_t price = read_price(ne, it);
                    it = it + 2;
                    skip_fixed(it, "\"q\":\"");
                    ne = std::find(it, ie, '\"');
                    count_t count = read_count(it, ne);
                    it = ne + 2;
                    skip_fixed(it, "\"s\":\"");
                    uint32_t direction;
                    if(skip_if_fixed(it, "BUY"))
                        direction = 1;
                    else {
                        skip_fixed(it, "SELL");
                        direction = 2;
                    }
                    it = std::find(it, ie, '}');
                    ++it;
                    add_trade(security_id, price, count, direction, etime, time);
                    if(it == ie || *it == ']')
                        break;
                    if(*it == ',')
                        ++it;
                }
                skip_fixed(it, "]}}");
                send_messages();
            }
        }
        else if(skip_if_fixed(it, ",\"code\":0,\"method\":\"subscribe\",\"channel\""))
        {
        }
        else if(skip_if_fixed(it, ",\"method\":\"public/heartbeat\",\"code\":0}"))
        {
            int64_t id = lexical_cast<int64_t>(ne, std::find(ne, it, ','));
            bs << "{\"id\":" << id << ",\"method\":\"public/respond-heartbeat\"}";
            send(wsi);
        }
        else
            throw std::runtime_error(es() % "unsupported message: " % str_holder(in, len));
    }
};

void proceed_crypto_com(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

void connect(lws_i& ls)
{
    lws_connect(ls, "stream.crypto.com", 443, "/v2/market");
}

