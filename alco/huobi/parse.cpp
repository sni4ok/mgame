/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "../alco.hpp"
#include "evie/profiler.hpp"

#include "utils.hpp"

#include <libwebsockets.h>

struct parser : stack_singleton<parser>
{
    tyra t;
    parser() : t(config::instance().push)
    {
    }
};

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

struct lws_i : parser
{
    volatile bool& can_run;
    lws_context* context;
    zlibe zlib;
    char buf[512];
    buf_stream bs;

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

    typedef const char* iterator;
    template<typename str>
    void skip_fixed(iterator& it, const str& v)
    {
        static_assert(std::is_array<str>::value);
        bool eq = std::equal(it, it + sizeof(v) - 1, v);
        if(unlikely(!eq))
            throw std::runtime_error(es() % "bad message: " % str_holder(it, 100));
        it += sizeof(v) - 1;
    }
    typedef my_basic_string<char, 64> string;
    struct impl;
    typedef void (lws_i::*func)(impl& i, ttime_t etime, iterator& it, iterator ie);
    struct impl
    {
        impl() : ask_price(), bid_price(), ask_count(), bid_count(), m_b(), m_t()
        {
        }
        impl(string&& symbol, security* sec, func f) : impl()
        {
            this->symbol = symbol;
            this->sec = sec;
            this->f = f;
        }
        string symbol;
        security* sec;
        func f;

        //bbo
        price_t ask_price, bid_price;
        count_t ask_count, bid_count;

        uint32_t m_b, m_t;
        bool operator<(const impl& i) const {
            return symbol < i.symbol;
        }
        bool operator<(const str_holder& s) const {
            return symbol < s;
        }
    };
    void add_order(impl& i, price_t price, count_t count, ttime_t etime, ttime_t time)
    {
        if(unlikely(i.m_b == pre_alloc))
            send_book(i);

        tyra_msg<message_book>& m = mb[i.m_b++];
        m.security_id = i.sec->mb.security_id;
        m.price = price;
        m.count = count;
        m.etime = etime;
        m.time = time;
    }
    void add_trade(impl& i, price_t price, count_t count, uint32_t direction, ttime_t etime, ttime_t time)
    {
        if(unlikely(i.m_t == pre_alloc))
            send_trades(i);

        tyra_msg<message_trade>& m = mt[i.m_t++];
        m.security_id = i.sec->mt.security_id;
        m.direction = direction;
        m.price = price;
        m.count = count;
        m.etime = etime;
        m.time = time;
    }
    void send_book(impl& i)
    {
        if(i.m_b) {
            MPROFILE("send_book")
            t.send(mb, i.m_b);
            i.m_b = 0;
        }
    }
    void send_trades(impl& i)
    {
        if(i.m_t) {
            MPROFILE("send_trades")
            t.send(mt, i.m_t);
            i.m_t = 0;
        }
    }
    void parse_bbo(impl& i, ttime_t etime, iterator& it, iterator ie)
    {
        MPROFILE("parse_bbo")

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
            add_order(i, i.ask_price, count_t(), etime, time);
        if(i.ask_price != ask_price || i.ask_count != ask_count)
            add_order(i, ask_price, ask_count, etime, time);

        if(i.bid_price != bid_price && i.bid_count != count_t())
            add_order(i, i.bid_price, count_t(), etime, time);
        if(i.bid_price != bid_price || i.bid_count != bid_count)
            add_order(i, bid_price, bid_count, etime, time);
       
        send_book(i);
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
            add_order(i, p, c, etime, time);
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
    void parse_orders(impl& i,ttime_t etime, iterator& it, iterator ie)
    {
        MPROFILE("parse_orders")
        parse_orders_impl(i, etime, get_cur_ttime(), it, ie);
        skip_fixed(it, end);
        send_book(i);
    }
    void parse_snapshot(impl& i, ttime_t etime, iterator& it, iterator ie)
    {
        MPROFILE("parse_snapshot")
        ttime_t time = get_cur_ttime();
        parse_orders_impl(i, etime, time, it, ie);
        it = std::find(it, ie, '}');
        skip_fixed(it, end);
        i.sec->mc.time = time;
        t.send(i.sec->mc);
        send_book(i);
    }
    void parse_trades(impl& i, ttime_t etime, iterator& it, iterator ie)
    {
        MPROFILE("parse_trades")

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
            add_trade(i, p, c, dir, etime, time);

            if(*it != ',') {
                skip_fixed(it, "]");
                break;
            }
            else ++it;
        }

        skip_fixed(it, end);
        send_trades(i);
    }
    std::vector<security> securities;
    std::vector<impl> parsers;

    static const uint32_t pre_alloc = 150;
    tyra_msg<message_book> mb[pre_alloc];
    tyra_msg<message_trade> mt[pre_alloc];
    
    std::vector<std::string> subscribes;
    lws_i(volatile bool& can_run) : can_run(can_run), context(), bs(buf, buf + sizeof(buf) - 1),
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
        bs.resize(LWS_PRE);
        config& c = config::instance();
        securities.reserve(c.tickers.size());
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
    void send(lws* wsi)
    {
        if(bs.size() != LWS_PRE) {
            int sz = bs.size() - LWS_PRE;
            char* ptr = bs.from + LWS_PRE;
            if(config::instance().log_lws)
                mlog(mlog::critical) << "lws_write " << str_holder(ptr, sz);
            int n = lws_write(wsi, (unsigned char*)ptr, sz, LWS_WRITE_TEXT);
                
            if(unlikely(sz != n)) {
                mlog(mlog::critical) << "lws_write ret: " << n << ", sz: " << sz;
                return;
            }
            bs.resize(LWS_PRE);
        }
    }
    void init(lws* wsi)
    {
        for(auto& s: subscribes) {
            bs << s;
            send(wsi);
        }
    }
    void proceed_impl(lws* wsi, void* in, size_t len)
    {
        str_holder str = zlib.decompress(in, len);
        if(config::instance().log_lws)
            mlog() << "lws proceed: " << str;
        if(unlikely(!str.size))
            return;
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
            return;
        }
        else if(unlikely(std::equal(error, error + sizeof(error) - 1, it)))
            throw std::runtime_error(es() % "error message: " % str);
        else {
            mlog(mlog::critical) << "unsupported message: " << str;
        }
    }
    int proceed(lws* wsi, void* in, size_t len)
    {
        try {
            proceed_impl(wsi, in, len);
        }
        catch(std::exception& e) {
            mlog() << "lws_i::proceed() " << e;
            throw;
            return -1;
        }
        return 0;
    }
    ~lws_i()
    {
        lws_context_destroy(context);
    }
};

static int lws_event_cb(lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
    switch (reason)
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            mlog() << "lws connection established";
            ((lws_i*)user)->init(wsi);
            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            if(config::instance().log_lws)
                mlog() << "lws receive len: " << len;
            return ((lws_i*)user)->proceed(wsi, in, len);
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
        {
            mlog() << "lws closed";
            return -1;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            lwsl_user("closed... \n");
            return 1;
        }
        default:
        {
            if(config::instance().log_lws)
                mlog() << "lws callback: " << int(reason) << ", len: " << len;
            break;
        }
    }
    return 0;
}

lws_context* create_context()
{
    int logs = LLL_ERR | LLL_WARN ;
    lws_set_log_level(logs, NULL);
    lws_context_creation_info info;
    memset(&info, 0, sizeof (info));
    info.port = CONTEXT_PORT_NO_LISTEN;

    lws_protocols protocols[] =
    {
        {"example-protocol", lws_event_cb, 0, 4096 * 100, 0, 0, 0},
        lws_protocols()
    };

    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.client_ssl_ca_filepath = "cert.pem";
    lws_context* context = lws_create_context(&info);
    if(!context)
        throw std::runtime_error("lws_create_context error");
    return context;
}

void connect(lws_i& ls)
{
	lws_client_connect_info ccinfo =lws_client_connect_info();
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
    try {
        while(can_run) {
            lws_i ls(can_run);
            ls.context = create_context();
            connect(ls);

            int n = 0;
            while(can_run && n >= 0) {
                n = lws_service(ls.context, 0);
            }
        }
    }
    catch(std::exception& e) {
        mlog() << "proceed_huobi " << e;
        if(can_run)
            usleep(5000 * 1000);
    }
}

