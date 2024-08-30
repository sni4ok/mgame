/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"

struct lws_i: sec_id_by_name<lws_impl>
{
    config& cfg;

    lws_i() : sec_id_by_name<lws_impl>(config::instance().push, config::instance().log_lws), cfg(config::instance())
    {
        my_stream sub;
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

    void proceed(lws* wsi, char_cit in, size_t len)
    {
        ttime_t time = cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder(in, len);
        char_cit it = in, ie = it + len;
        skip_fixed(it, "{\"id\":");
        char_cit ne = it;
        it = find(it, ie, ',');
        if(skip_if_fixed(it, ",\"method\":\"subscribe\",\"code\":0,\"result\":{\"instrument_name\":\""))
        {
            bool book = false;
            ne = find(it, ie, '\"');
            uint32_t security_id = get_security_id(it, ne, time);
            it = ne + 1;
            skip_fixed(it, ",\"subscription\":\"");
            if(skip_if_fixed(it, "book"))
                book = true;
            else
                skip_fixed(it, "trade");

            search_and_skip_fixed(it, ie, "\"data\":[");
            if(book)
            {
                ttime_t etime;
                if(len > 80)
                {
                    ne = ie - 80;
                    search_and_skip_fixed(ne, ie, "\"t\":");
                    etime.value = lexical_cast<uint64_t>(ne, find(ne, ie, ',')) * (ttime_t::frac / 1000);
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
                            ne = find(it, ie, '"');
                            price_t price = read_price(it, ne);
                            it = ne + 2;
                            skip_fixed(it, "\"");
                            ne = find(it, ie, '"');
                            count_t count = read_count(it, ne);
                            count.value = -count.value;
                            add_order(security_id, price.value, price, count, etime, time);
                            it = find(ne, ie, ']');
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
                            ne = find(it, ie, '"');
                            price_t price = read_price(it, ne);
                            it = ne + 2;
                            skip_fixed(it, "\"");
                            ne = find(it, ie, '"');
                            count_t count = read_count(it, ne);
                            add_order(security_id, price.value, price, count, etime, time);
                            it = find(ne, ie, ']');
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
                it = find(it, ie, ']');
                skip_fixed(it, "]}}");
                send_messages();
            }
            else
            {
                for(;;)
                {
                    skip_fixed(it, "{\"d\":\"");
                    search_and_skip_fixed(it, ie, "\",\"t\":");
                    ne = find(it, ie, ',');
                    ttime_t etime;
                    etime.value = lexical_cast<uint64_t>(it, ne) * (ttime_t::frac / 1000);
                    skip_fixed(ne, ",\"p\":\"");
                    it = find(ne, ie, '\"');
                    price_t price = read_price(ne, it);
                    it = it + 2;
                    skip_fixed(it, "\"q\":\"");
                    ne = find(it, ie, '\"');
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
                    it = find(it, ie, '}');
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
        else if(skip_if_fixed(it, ",\"method\":\"subscribe\",\"code\":0,\"channel\""))
        {
        }
        else if(skip_if_fixed(it, ",\"method\":\"public/heartbeat\",\"code\":0}"))
        {
            int64_t id = lexical_cast<int64_t>(ne, find(ne, it, ','));
            bs << "{\"id\":" << id << ",\"method\":\"public/respond-heartbeat\"}";
            send(wsi);
        }
        else
            throw mexception(es() % "unsupported message: " % str_holder(in, len));
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

