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
    typedef void (lws_i::*func)(impl& i, ttime_t time, iterator& it, iterator ie);
    struct impl
    {
        uint32_t security_id;
        func f;
        uint32_t high;
        impl() : high()
        {
        }
    };
    const uint64_t ping;
    ttime_t ping_t;

    fmap<uint32_t, impl> parsers; //channel, impl
    lws_i() :
        cfg(config::instance()),
        subscribed("\"subscribed\",\"channel\":"),
        trade("\"trades\",\"chanId\":"),
        book("\"book\",\"chanId\":"),
        symbol("\"symbol\":\""),
        event("\"event\":"),
        ping(uint64_t(cfg.ping) * ttime_t::frac), ping_t(get_cur_ttime())
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
    void parse_trades(impl& i, ttime_t time, iterator& it, iterator ie)
    {
        if(*(it + 1) == '[')
        {
            //trade book after initialize
            it = ie - 3;
            skip_fixed(it, "]]]");
            return;
        }
        skip_fixed(it, "[");
        iterator ne = std::find(it, ie, ',');
        uint32_t id = my_cvt::atoi<uint32_t>(it, ne - it);
        ++ne;
        ttime_t etime = {my_cvt::atoi<uint64_t>(ne, 13) * (ttime_t::frac / 1000)};
        ne += 13;
        skip_fixed(ne, ",");
        it = std::find(ne, ie, ',');
        count_t count = read_count(ne, it);
        int dir = 1;
        if(count.value < 0)
        {
            dir = 2;
            count.value = -count.value;
        }
        skip_fixed(it, ",");
        ne = std::find(it, ie, ']');
        price_t price = read_price(it, ne);
        
        if(likely(*ne == ']' && *(ne + 1) == ']'))
            it = ne + 2;
        
        if(id > i.high)
        {
            add_trade(i.security_id, price, count, dir, etime, time);
            i.high = id;
            send_messages();
        }
    }
    void parse_orders(impl& i, ttime_t time, iterator& it, iterator ie)
    {
        bool one = *(it + 1) != '[';

        if(!one)
            skip_fixed(it, "[");
        iterator ne;
        for(;;)
        {
            skip_fixed(it, "[");
            ne = std::find(it, ie, ',');
            if(prec_R0)
            {
                int64_t level_id = my_cvt::atoi<int64_t>(it, ne - it);
                ++ne;
                it = std::find(ne, ie, ',');
                price_t price = read_price(ne, it);
                ++it;
                ne = std::find(it, ie, ']');
                count_t amount = read_count(it, ne);
                if(price != price_t())
                    add_order(i.security_id, level_id, price, amount, ttime_t(), time);
                else
                    add_order(i.security_id, level_id, price, count_t(), ttime_t(), time);
            }
            else
            {
                price_t price = read_price(it, ne);
                ++ne;
                it = std::find(ne, ie, ',');
                uint32_t count = my_cvt::atoi<uint32_t>(ne, it - ne);
                ++it;
                ne = std::find(it, ie, ']');
                count_t amount = read_count(it, ne);
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
    void add_channel(uint32_t channel, str_holder ticker, bool is_trades, ttime_t time)
    {
        mlog() << "add_channel: " << channel << ", ticker: " << ticker << ", is_trades: " << is_trades;
        impl& i = parsers[channel];
        i.security_id = get_security_id(ticker.str, ticker.str + ticker.size, time);
        if(is_trades)
            i.f = &lws_i::parse_trades;
        else
            i.f = &lws_i::parse_orders;
    }
    void proceed(lws* wsi, void* in, size_t len)
    {
        ttime_t time = get_cur_ttime();
        if(cfg.log_lws)
            mlog() << "lws proceed: " << str_holder((const char*)in, len);
        iterator it = (iterator)in, ie = it + len;

        if(likely(*it == '['))
        {
            ++it;
            iterator ne = std::find(it, ie, ',');
            uint32_t channel = my_cvt::atoi<uint32_t>(it, ne - it);
            skip_fixed(ne, ",");
            ne = std::find(ne, ie, '[');

            if(likely(ne != ie)) {
                impl& i = parsers.at(channel);
                ((this)->*(i.f))(i, time, ne, ie);
                if(ne != ie)
                    throw std::runtime_error(es() % "parsing market message error: " % str_holder((const char*)in, len));
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
                    iterator ne = std::find(it, ie, ',');
                    uint32_t channel = my_cvt::atoi<uint32_t>(it, ne - it);
                    ++ne;
                    skip_fixed(ne, symbol);
                    it = std::find(ne, ie, '\"');

                    str_holder ticker(ne, it - ne);
                    add_channel(channel, ticker, is_trades, time);
                    it = std::find(it, ie, '}') + 1;
                    if(it != ie)
                        throw std::runtime_error(es() % "parsing logic error message: " % str_holder((const char*)in, len));
                }
            }
            else
                mlog(mlog::critical) << "unsupported message: " << str_holder(it, len);
        }
        else
            mlog(mlog::critical) << "unsupported message: " << str_holder(it, len);

        if(ping) {
            if(time.value > ping_t.value + ping) {
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
	lws_client_connect_info ccinfo =lws_client_connect_info();
	ccinfo.context = ls.context;
	ccinfo.address = "api-pub.bitfinex.com";
	ccinfo.port = 443;
    ccinfo.path = "/ws/2";
	
    ccinfo.userdata = (void*)&ls;
	ccinfo.protocol = "ws";
	ccinfo.origin = "origin";
	ccinfo.host = ccinfo.address;
	ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	lws_client_connect_via_info(&ccinfo);
}

