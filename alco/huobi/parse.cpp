/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../lws.hpp"
#include "../utils.hpp"
#include "utils.hpp"

struct lws_i : sec_id_by_name<lws_impl>
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
    typedef void (lws_i::*func)(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie);
    struct impl
    {
        impl() : security_id(), f()
        {
        }
        impl(ticker security, func f) : security(security), security_id(), f(f)
        {
        }
        ticker security;
        uint32_t security_id;
        func f;
    };
    fmap<string, impl> parsers;

    void parse_bbo(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie)
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

        add_order(i.security_id, 2, ask_price, ask_count, etime, time);
        add_order(i.security_id, 1, bid_price, bid_count, etime, time);
       
        send_messages();
    }
    void parse_orders_impl(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie, bool a)
    {
        if(*it != ']')
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
                add_order(i.security_id, p.value, p, c, etime, time);
                if(*it != ',') {
                    skip_fixed(it, "]");
                    break;
                }
                else ++it;
            }
        }
        else
            ++it;
    }
    void parse_orders_impl(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie)
    {
        search_and_skip_fixed(it, ie, "prevSeqNum\":");
        search_and_skip_fixed(it, ie, ",\"");

        bool have_asks = false;
        if(skip_if_fixed(it, asks)) {
            parse_orders_impl(i, etime, time, it, ie, true);
            have_asks = true;
        }

        search_and_skip_fixed(it, ie, bids);
        parse_orders_impl(i, etime, time, it, ie, false);

        if(!have_asks) {
            search_and_skip_fixed(it, ie, asks);
            parse_orders_impl(i, etime, time, it, ie, true);
        }
    }
    void parse_orders(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie)
    {
        parse_orders_impl(i, etime, time, it, ie);
        skip_fixed(it, end);
        send_messages();
    }
    void parse_snapshot(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie)
    {
        add_clean(i.security_id, etime, time);
        parse_orders_impl(i, etime, time, it, ie);
        it = std::find(it, ie, '}');
        skip_fixed(it, end);
        send_messages();
    }
    void parse_trades(impl& i, ttime_t etime, ttime_t time, iterator& it, iterator ie)
    {
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
        send_messages();
    }
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
            if(c.snapshot) {
                parsers[v + ".depth." + c.step] = impl(v, &lws_i::parse_snapshot);
                subscribes.push_back("{\"sub\":\"market." + v + ".depth." + c.step + "\",\"id\":\"snapshot_" + v + "\"}");
            }
            if(c.orders) {
                parsers[v + ".mbp." + c.levels] = impl(v, &lws_i::parse_orders);
                subscribes.push_back("{\"sub\":\"market." + v + ".mbp." + c.levels + "\",\"id\":\"orders_" + v + "\"}");
            }
            if(c.bbo) {
                parsers[v + ".bbo"] = impl(v, &lws_i::parse_bbo);
                subscribes.push_back("{\"sub\":\"market." + v + ".bbo\",\"id\":\"bbo_" + v + "\"}");
            }
            if(c.trades) {
                parsers[v + ".trade.detail"] = impl(v, &lws_i::parse_trades);
                subscribes.push_back("{\"sub\":\"market." + v + ".trade.detail\",\"id\":\"trades_" + v + "\"}");
            }
        }
    }
    void proceed(lws* wsi, void* in, size_t len)
    {
        ttime_t time = get_cur_ttime();
        str_holder str = zlib.decompress(in, len);
        if(config::instance().log_lws)
            mlog() << "lws proceed: " << str;
        iterator it = str.str, ie = str.str + str.size;
        if(unlikely(*it != '{' || *(it + 1) != '\"'))
            throw std::runtime_error(es() % "bad message: " % str);
        it = it + 2;

        if(likely(std::equal(ch, ch + sizeof(ch) - 1, it)))
        {
            it = it + sizeof(ch) - 1;
            iterator ne = std::find(it, ie, '\"');
            str_holder symbol(it, ne - it);
            auto i = parsers.find(symbol);
            if(unlikely(i == parsers.end()))
                throw std::runtime_error(es() % "unknown symbol in market message: " % str);
            if(unlikely(!i->second.security_id))
                i->second.security_id = get_security_id(i->second.security.begin(), i->second.security.end(), time);
            ++ne;
            skip_fixed(ne, ts);
            ttime_t etime = {my_cvt::atoi<uint64_t>(ne, 13) * (ttime_t::frac / 1000)};
            ne += 13;
            skip_fixed(ne, tick);
            ((this)->*(i->second.f))(i->second, etime, time, ne, ie);
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
    }
};

void proceed_huobi(volatile bool& can_run)
{
    return proceed_lws_parser<lws_i>(can_run);
}

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

