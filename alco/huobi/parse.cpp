/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "evie/profiler.hpp"

#include "../alco.hpp"
#include "../lws.hpp"
#include "utils.hpp"

void test_impl(const char* a1, const char* a2)
{
    count_t v1 = read_count(a1, a1 + strlen(a1));
    count_t v2 = read_count(a2, a2 + strlen(a2));
    assert(v1.value == v2.value);
    my_unused(v1, v2);
}
void amount_test()
{
    test_impl("-0.5E-50", "0");
    test_impl("5.6", "0.56E1");
    test_impl("0.056", "0.56E-1");
    test_impl("0.56", "5.6E-1");
    test_impl("-0.056", "-0.56E-1");
    test_impl("-5.6", "-0.56E1");
    test_impl("-0.56", "-5.6E-1");
    test_impl("560", "56E1");

    const char* v = "9.79380846343861E-4";
    count_t c = read_count(v, v + strlen(v));
    my_unused(c);
}

struct lws_i : lws_impl
{
    zlibe zlib;

    char ch[13];
    char ts[7];
    char tick[11];
    char ask[8];
    char askSize[12];
    char bid[8];
    char bidSize[12];
    char end[3];

    char id[7];
    char amount[11];
    char price[10];
    char direction[15];
    char sell[7];
    char buy[6];
   
    char bids[8];
    char asks[8];

    char ping[7];
    char error[16];

    typedef my_basic_string<char, 64> string;
    struct impl;
    typedef void (lws_i::*func)(impl& i, ttime_t etime, iterator& it, iterator ie);
    struct impl
    {
        impl() : ask_price(), bid_price(), ask_count(), bid_count()
        {
        }
        impl(string&& symbol, security* sec, func f) : impl()
        {
            this->symbol = symbol;
            this->sec = sec;
            this->f = f;
            security_id = this->sec->mi.security_id;
        }
        string symbol;
        uint32_t security_id;
        security* sec;
        func f;

        //bbo
        price_t ask_price, bid_price;
        count_t ask_count, bid_count;

        bool operator<(const impl& i) const {
            return symbol < i.symbol;
        }
        bool operator<(const str_holder& s) const {
            return symbol < s;
        }
    };
    void parse_bbo(impl& i, ttime_t etime, iterator& it, iterator ie)
    {
        //MPROFILE("parse_bbo")

        it = std::find(it, ie, ',');
        skip_fixed(it, ask);
        iterator ne = std::find(it, ie, ',');
        price_t ask_price = read_price(it, ne);
        skip_fixed(ne, askSize);
        it = std::find(ne, ie, ',');
        count_t ask_count = read_count(ne, it);
        ask_count.value = -ask_count.value;
        skip_fixed(it, bid);
        ne = std::find(it, ie, ',');
        price_t bid_price = read_price(it, ne);
        skip_fixed(ne, bidSize);
        it = std::find(ne, ie, ',');
        count_t bid_count = read_count(ne, it);

        it = std::find(it, ie, '}');
        skip_fixed(it, end);

        ttime_t time = get_cur_ttime();
        if(i.ask_price != ask_price && i.ask_count != count_t())
            add_order(i.security_id, i.ask_price, count_t(), etime, time);
        if(i.ask_price != ask_price || i.ask_count != ask_count)
            add_order(i.security_id, ask_price, ask_count, etime, time);

        if(i.bid_price != bid_price && i.bid_count != count_t())
            add_order(i.security_id, i.bid_price, count_t(), etime, time);
        if(i.bid_price != bid_price || i.bid_count != bid_count)
            add_order(i.security_id, bid_price, bid_count, etime, time);
       
        send_book();
        i.ask_price = ask_price;
        i.ask_count = ask_count;
        i.bid_price = bid_price;
        i.bid_count = bid_count;
    }
    void parse_orders_impl(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie, bool a)
    {
        for(;;) {
            skip_fixed(it, "[");
            iterator ne = std::find(it, ie, ',');
            price_t p = read_price(it, ne);
            skip_fixed(ne, ",");
            it = std::find(ne, ie, ']');
            count_t c = read_count(ne, it);
            if(a)
                c.value = -c.value;
            skip_fixed(it, "]");
            add_order(i.security_id, p, c, etime, time);
            if(*it != ',') {
                skip_fixed(it, "]");
                break;
            }
            else ++it;
        }
    }
    void parse_orders_impl(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie)
    {
        it = std::search(it, ie, bids, bids + sizeof(bids) - 1);
        skip_fixed(it, bids);
        if(*it != ']')
            parse_orders_impl(i, etime, time, it, ie, false);
        else
            ++it;

        it = std::search(it, ie, asks, asks + sizeof(asks) - 1);
        skip_fixed(it, asks);
        if(*it != ']')
            parse_orders_impl(i, etime, time, it, ie, true);
        else
            ++it;
    }
    void parse_orders(impl& i, ttime_t etime, iterator& it, iterator ie)
    {
        //MPROFILE("parse_orders")

        parse_orders_impl(i, etime, get_cur_ttime(), it, ie);
        skip_fixed(it, end);
        send_book();
    }
    void parse_snapshot(impl& i, ttime_t etime, iterator& it, iterator ie)
    {
        //MPROFILE("parse_snapshot")

        ttime_t time = get_cur_ttime();
        parse_orders_impl(i, etime, time, it, ie);
        it = std::find(it, ie, '}');
        skip_fixed(it, end);
        i.sec->mc.time = time;
        t.send(i.sec->mc);
        send_book();
    }
    void parse_trades(impl& i, ttime_t etime, iterator& it, iterator ie)
    {
        //MPROFILE("parse_trades")

        ttime_t time = get_cur_ttime();
        it = std::find(it, ie, '[') + 1;
        for(;;)
        {
            skip_fixed(it, id);
            it = std::find(it, ie, ',');
            it = std::find(it + 1, ie, ',');
            it = std::find(it + 1, ie, ',');
            skip_fixed(it, amount);
            iterator ne = std::find(it, ie, ',');
            count_t c = read_count(it, ne);
            skip_fixed(ne, price);
            it = std::find(ne, ie, ',');
            price_t p = read_price(ne, it);
            skip_fixed(it, direction);
            uint32_t dir;
            if(*it == 's') {
                skip_fixed(it, sell);
                dir = 2;
            } else {
                skip_fixed(it, buy);
                dir = 1;
            }
            add_trade(i.security_id, p, c, dir, etime, time);

            if(*it != ',') {
                skip_fixed(it, "]");
                break;
            }
            else ++it;
        }

        skip_fixed(it, end);
        send_trades();
    }
    std::vector<security> securities;
    std::vector<impl> parsers;

    lws_i() :
        ch("ch\":\"market."),
        ts(",\"ts\":"),
        tick(",\"tick\":{\""),
        ask(",\"ask\":"), askSize(",\"askSize\":"),
        bid(",\"bid\":"), bidSize(",\"bidSize\":"),
        end("}}"),
        id("{\"id\":"),
        amount(",\"amount\":"),
        price(",\"price\":"),
        direction(",\"direction\":\""),
        sell("sell\"}"),
        buy("buy\"}"),
        bids("bids\":["),
        asks("asks\":["),
        ping("ping\":"),
        error("status\":\"error\"")
    {
        config& c = config::instance();
        for(auto& v: c.tickers) {
            security sec;
            sec.init(c.exchange_id, c.feed_id, v);
            securities.push_back(std::move(sec));
            security& s = securities.back();
            t.send(s.mi);

            if(c.snapshot) {
                parsers.push_back({v + ".depth." + c.step, &s, &lws_i::parse_snapshot});
                subscribes.push_back("{\"sub\":\"market." + v + ".depth." + c.step + "\",\"id\":\"snapshot_" + v + "\"}");
            }
            if(c.orders) {
                parsers.push_back({v + ".mbp." + c.levels, &s, &lws_i::parse_orders});
                subscribes.push_back("{\"sub\":\"market." + v + ".mbp." + c.levels + "\",\"id\":\"orders_" + v + "\"}");
            }
            if(c.bbo) {
                parsers.push_back({v + ".bbo", &s, &lws_i::parse_bbo});
                subscribes.push_back("{\"sub\":\"market." + v + ".bbo\",\"id\":\"bbo_" + v + "\"}");
            }
            if(c.trades) {
                parsers.push_back({v + ".trade.detail", &s, &lws_i::parse_trades});
                subscribes.push_back("{\"sub\":\"market." + v + ".trade.detail\",\"id\":\"trades_" + v + "\"}");
            }
        }
        std::sort(parsers.begin(), parsers.end());
    }
    int proceed(lws* wsi, void* in, size_t len)
    {
        str_holder str = zlib.decompress(in, len);
        if(config::instance().log_lws)
            mlog() << "lws proceed: " << str;
        if(unlikely(!str.size))
            return 0;
        iterator it = str.str, ie = str.str + str.size;
        if(unlikely(*it != '{' || *(it + 1) != '\"'))
            throw std::runtime_error(es() % "bad message: " % str);
        it = it + 2;

        if(likely(std::equal(ch, ch + sizeof(ch) - 1, it)))
        {
            it = it + sizeof(ch) - 1;
            iterator ne = std::find(it, ie, '\"');
            str_holder symbol(it, ne - it);
            std::vector<impl>::iterator i = std::lower_bound(parsers.begin(), parsers.end(), symbol);
            if(unlikely(i == parsers.end() || i->symbol != symbol))
                throw std::runtime_error(es() % "unknown symbol in market message: " % str);
            ++ne;
            skip_fixed(ne, ts);
            ttime_t etime = {my_cvt::atoi<uint64_t>(ne, 13) * (ttime_t::frac / 1000)};
            ne += 13;
            skip_fixed(ne, tick);
            ((this)->*(i->f))(*i, etime, ne, ie);
            if(ne != ie)
                throw std::runtime_error(es() % "parsing market message error: " % str);
        }
        else if(std::equal(ping, ping + sizeof(ping) - 1, it))
        {
            it = it + sizeof(ping) - 1;
            if(unlikely(*(it + 13) != '}' || it + 14 != ie))
                throw std::runtime_error(es() % "bad ping message: " % str);

            bs << "{\"pong\":";
            bs.write(it, 13);
            bs << "}";
            send(wsi);
        }
        else if(unlikely(std::equal(error, error + sizeof(error) - 1, it)))
            throw std::runtime_error(es() % "error message: " % str);
        else {
            mlog(mlog::critical) << "unsupported message: " << str;
        }
        return 0;
    }
};

void connect(lws_i& ls)
{
	lws_client_connect_info ccinfo = lws_client_connect_info();
	ccinfo.context = ls.context;
	ccinfo.address = "api.huobi.pro";
	ccinfo.port = 443;
    ccinfo.path = "/ws";
	
    ccinfo.userdata = (void*)&ls;
	ccinfo.protocol = "ws";
	ccinfo.origin = "origin";
	ccinfo.host = ccinfo.address;
	ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	lws_client_connect_via_info(&ccinfo);
}

void proceed_huobi(volatile bool& can_run)
{
    size_t msz = sizeof(message);
    my_unused(msz);
    amount_test();
    while(can_run) {
        try {
            lws_i ls;
            ls.context = create_context<lws_i>();
            connect(ls);

            int n = 0;
            while(can_run && n >= 0) {
                n = lws_service(ls.context, 0);
            }
        } catch(std::exception& e) {
            mlog() << "proceed_huobi " << e;
            if(can_run)
                usleep(5000 * 1000);
        }
    }
}

