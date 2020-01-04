/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../alco.hpp"
#include "../lws.hpp"


struct lws_i : lws_impl
{
    std::vector<security> securities;
    
    char subscribed[24];
    char trade[19];
    char book[17];
    char symbol[11];
    char event[9];

    struct impl;
    typedef void (lws_i::*func)(impl& i, iterator& it, iterator ie);
    struct impl
    {
        uint32_t security_id;
        func f;
        uint32_t high;
        impl() : high()
        {
        }
    };
    const uint32_t ping;
    time_t ping_t;

    fmap<uint32_t, impl> parsers; //channel, impl
    lws_i() :
        subscribed("\"subscribed\",\"channel\":"),
        trade("\"trades\",\"chanId\":"),
        book("\"book\",\"chanId\":"),
        symbol("\"symbol\":\""),
        event("\"event\":"),
        ping(config::instance().ping), ping_t(time(NULL))
    {
        config& c = config::instance();
        for(auto& v: c.tickers) {
            security sec;
            sec.init(c.exchange_id, c.feed_id, v);
            securities.push_back(std::move(sec));
            security& s = securities.back();
            t.send(s.mi);

            if(c.trades) {
                subscribes.push_back("{\"event\":\"subscribe\",\"channel\":\"trades\",\"symbol\":\"" + v + "\"}");
            }
            if(c.orders) {
                subscribes.push_back("{\"event\":\"subscribe\",\"channel\":\"book\",\"symbol\":\""
                    + v + "\",\"prec\":\"" + c.precision + "\",\"freq\":\"" + c.frequency + "\",\"len\":\"" + c.length + "\"}");
            }
        }
    }
    void parse_trades(impl& i, iterator& it, iterator ie)
    {
        mlog() << "parse_trades: " << str_holder(it, ie - it);
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
            add_trade(i.security_id, price, count, dir, etime, get_cur_ttime());
            i.high = id;
            send_trades();
        }
    }
    void parse_orders(impl& i, iterator& it, iterator ie)
    {
        mlog() << "parse_orders: " << str_holder(it, ie - it);
        bool one = *(it + 1) != '[';

        if(!one)
            skip_fixed(it, "[");
        iterator ne;
        ttime_t ctime = get_cur_ttime();
        for(;;)
        {
            skip_fixed(it, "[");
            ne = std::find(it, ie, ',');
            price_t price = read_price(it, ne);
            ne = std::find(ne + 1, ie, ',') + 1; //skip count (number orders on current price level)
            it = std::find(ne, ie, ']');
            count_t count = read_count(ne, it);
            add_order(i.security_id, price, count, ttime_t(), ctime);
            ++it;
            if(*it != ',')
                break;
            else
                ++it;
        }
        if(one)
            skip_fixed(it, "]");
        else
            skip_fixed(it, "]]");
        send_book();
    }
    void add_channel(uint32_t channel, str_holder ticker, bool is_trades)
    {
        mlog() << "add_channel: " << channel << ", ticker: " << ticker << ", is_trades: " << is_trades;
        for(auto& sec: securities) {
            if(str_holder(sec.mi.security) == ticker)
            {
                impl& i = parsers[channel];
                i.security_id = sec.mi.security_id;
                if(is_trades)
                    i.f = &lws_i::parse_trades;
                else
                    i.f = &lws_i::parse_orders;
                return;
            }
        }
        throw std::runtime_error(es() % "add_channel error, channel: " % channel % ", ticker: " % ticker);
    }
    int proceed(lws* wsi, void* in, size_t len)
    {
        if(config::instance().log_lws)
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
                ((this)->*(i.f))(i, ne, ie);
                if(ne != ie)
                    throw std::runtime_error(es() % "parsing market message error: " % str_holder((const char*)in, len));
            }
            else {
                //possible "hb" message
                t.ping();
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
                    add_channel(channel, ticker, is_trades);
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
            time_t cur_t = time(NULL);
            if(cur_t > ping_t + ping) {
                ping_t = cur_t;
                bs << "{\"event\":\"ping\",\"cid\":\"" << ping_t << "\"}";
                send(wsi);
            }
        }
        return 0;
    }
};

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

void proceed_bitfinex(volatile bool& can_run)
{
    while(can_run) {
        try {
            lws_i ls;
            ls.context = create_context<lws_i>();
            connect(ls);

            int n = 0;
            while(can_run && n >= 0) {
                n = lws_service(ls.context, 0);
            }
        }
        catch(std::exception& e) {
            mlog() << "proceed_bitfinex " << e;
            if(can_run)
                usleep(5000 * 1000);
        }
    }
}

