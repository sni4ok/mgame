/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "parse.hpp"
#include "config.hpp"

#include "../alco.hpp"

#include "evie/curl.hpp"

#include <memory>
#include <unistd.h>

struct binary_dumper : stack_singleton<binary_dumper>
{
    std::ofstream s;
    binary_dumper()
    {
    }
    void init(const std::string& fname)
    {
        s.open(fname, std::ofstream::out | std::ofstream::trunc | std::ios::binary);
        if(!s)
            throw std::runtime_error(es() % "open file \"" % fname % "\" error");
    }
    void write(const char* ptr, uint32_t size)
    {
        s << get_cur_ttime() << ":\n";
        s.write(ptr, size);
        s.write("\n\n", 2);
        s.flush();
    }
};

const char error_beg[] = "[\"error\",";
struct error
{
    std::string msg;
    size_t parse(const char* ptr, size_t size)
    {
        if(!std::equal(error_beg, error_beg + sizeof(error_beg) - 1, ptr))
            throw std::runtime_error(es() % "bad error exchange msg: " % std::string(ptr, ptr + std::min<size_t>(size, 20)));
        auto p = ptr + sizeof(error_beg) - 1;
        auto t = std::find(p, ptr + size, ']');
        if(t == ptr + size)
            throw std::runtime_error(es() % "bad error exchange msg: " % std::string(ptr, ptr + std::min<size_t>(size, 20)));
        msg = std::string(p, t);
        return t + 1 - ptr;
    }
};

void proceed_error(const char* ptr, uint32_t size)
{
//["error",10020,"symbol: invalid"]
    if(size > 16) {
        if(ptr[2] == 'e') {
            error e;
            e.parse(ptr, size);
            throw std::runtime_error(es() % "exchange error: " % e.msg);
        }
    }
}

price_t get_price(const char* f, const char* t)
{
    return {int64_t(my_cvt::atoi<uint64_t>(f, t - f)) * price_t::frac};
}

count_t get_count(const char* f, const char* t)
{
    bool minus = false;
    if(*f == '-') {
        minus = true;
        ++f;
    }
    const char* p = std::find(f, t, '.');
    int64_t v = my_cvt::atoi<uint32_t>(f, p - f);
    if(p == t)
        return {minus ? (-v * count_t::frac) : (v * count_t::frac)};
    ++p;
    uint32_t sz = t - p;
    int64_t c = my_cvt::atoi<uint32_t>(p, sz);
    for(uint32_t i = sz; i < 8; ++i)
        c *= 10;
    return {minus ? (-v * count_t::frac - c) : (v * count_t::frac + c)};
}

int32_t get_amount(const char* f, const char* t)
{
    return {my_cvt::atoi<int32_t>(f, t - f)};
}

struct parser : stack_singleton<parser>
{
    tyra t;
    parser() : t(config::instance().push)
    {
    }
    struct security : ::security
    {
        char tail_book[128], tail_trades[128];
        size_t tail_book_sz, tail_trades_sz;

        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> cb, ct;
        std::map<price_t, count_t> old_ob, cur_ob;

        security() : tail_book_sz(), tail_trades_sz(), cb(curl_easy_init(), &curl_easy_cleanup),
            ct(curl_easy_init(), &curl_easy_cleanup)
        {
        }
        void push_flush()
        {
            if(cur_ob.empty())
                return;
            for(auto& v: old_ob)
                v.second.value = 0;
            for(const auto& v: cur_ob)
                old_ob[v.first].value = v.second.value;

            uint32_t sz = old_ob.size();
            //mlog() << "sz:" << sz;
            if(sz) {
                for(auto& v: old_ob) {
                    proceed_book(parser::instance().t, v.first, v.second, ttime_t());
                    --sz;
                    if(!sz)
                        parser::instance().t.flush();
                }
            }
            old_ob.clear();
            old_ob.swap(cur_ob);
        }
    };
    std::vector<security> secs;

    uint32_t init(const std::string& ticker)
    {
        security sec;
        sec.init("bitfinex", config::instance().feed, ticker);
        t.send(sec.mi);

        secs.push_back(std::move(sec));
        return secs.size() - 1;
    }
};

size_t proceed_book(uint32_t sec_id, char* ptr, uint32_t size)
{
    auto& sec = (parser::instance().secs)[sec_id];
    auto& ob = sec.cur_ob;
    if(ptr[2] == 'e') //error, hack)
        return 0;

    char* beg = ptr;
    //[[6935,2,1.04808224],
	//price,, count
    auto ie = ptr + size;
    if(ptr[1] == '['){
        if(*ptr == '[')
            sec.push_flush();
        ++ptr;
    }

    price_t p;
    count_t c;
    //bool flush;
    for(;;) {
        assert(*ptr == '[');
        auto i = std::find(ptr, ie, ']');
        if(i == ie)
            return ptr - beg;
        ++ptr;
        auto z = std::find(ptr, ie, ',');
        *z = char(); //remove
        p = get_price(ptr, z);
        ++z;
        auto z2 = std::find(z, ie, ',');
        /*
        don't need amount
        *z2 = char(); //remove
        a = get_amount(z, z2);
        */
        ++z2;
        auto z3 = std::find(z2, ie, ']');
        *z3 = char(); //remove
        c = get_count(z2, z3);
        
        //if((ie - z3) > 30)
        //    flush = false;
        //else
        //    flush = std::find(z3, ie, ']') == ie;
        //parser::instance().proceed_book(sec_id, p, c, flush);
        ob[p] = c;

        ptr = i + 1;
        //mlog() << "p: " << p << ", a: " << a << ", c: " << c;
        if(ptr == ie || ptr + 1 == ie)
            return ptr - beg;
        if(*ptr == ',')
            ++ptr;
    }
    return ptr - beg;
}

uint32_t get_sec_id(void* p)
{
    return uint32_t((uint64_t)p);
}

size_t callback_book(void *contents, size_t sz, size_t nmemb, void* p)
{
    uint32_t sec_id = get_sec_id(p);
    auto& sec = (parser::instance().secs)[sec_id];
    auto& tail = sec.tail_book;
    auto& tail_sz = sec.tail_book_sz;

    size_t size = sz * nmemb;
    char *ptr = (char*)contents, *ie = ptr + size;
    
    if(config::instance().curl_log) {
        mlog() << "proceed_book: " << sz << "x" << nmemb << ", tail_sz: " << tail_sz << ", tail: " << std::string(tail, tail + tail_sz);
        binary_dumper::instance().write(ptr, size);
    }

    if(tail_sz) {
        auto p = std::find(ptr, ie, ']');
        if(p == ie || (p - ptr) + 1 + tail_sz > sizeof(tail))
            throw std::runtime_error(es() % "bad msg, tail: " % std::string(tail, tail + tail_sz));
        ++p;
        std::copy(ptr, p, &tail[tail_sz]);
        size_t head_sz = p - ptr;
        tail_sz += head_sz;
        ptr += head_sz;
        size -= head_sz;
        size_t c = proceed_book(sec_id, tail, tail_sz);
        if(c != tail_sz)
            throw std::runtime_error(es() % "bad msg: c: " % c % " t_sz: " % tail_sz % ", tail: " % std::string(tail, tail + tail_sz));
        tail_sz = 0;
    }

    size_t c = proceed_book(sec_id, ptr, size);
    if(!c)
        proceed_error(ptr, size);

    if(c != size) {
        assert(!tail_sz);
        uint32_t nc = 0;
        while(c != size && *(ptr + c) == ']')
            ++c, ++nc;
        if(nc >= 2)
            sec.push_flush();

        std::copy(ptr + c, ptr + size, tail);
        tail_sz = size - c;
    }

    return sz * nmemb;
}

const std::string api = "https://api.bitfinex.com/v2";
curl_err e;
size_t callback_trades(void *contents, size_t sz, size_t nmemb, void* p)
{
    uint32_t sec_id = get_sec_id(p);
    auto& sec = (parser::instance().secs)[sec_id];
    auto& tail = sec.tail_trades;
    auto& tail_sz = sec.tail_trades_sz;
    
    size_t size = sz * nmemb;
    char *ptr = (char*)contents;
    
    if(config::instance().curl_log) {
        mlog() << "proceed_trade: " << sz << "x" << nmemb << ", tail_sz: " << tail_sz << ", tail: " << std::string(tail, tail + tail_sz);
        binary_dumper::instance().write(ptr, size);
    }
    
    return size;
}

void init_book(uint32_t sec_id, const std::string& symbol, volatile bool& can_run)
{
    //uint32_t sec_id = parser::instance().init(symbol);
    const config& cfg = config::instance();
    std::string req = api + "/book/t" + symbol + "/"  + cfg.precision;
    if(!cfg.deep.empty())
        req += "?len=" + cfg.deep;

    mlog() << "curl_req: " << req;
    parser::security& s = parser::instance().secs[sec_id];
    CURL* c = s.cb.get();
    e = curl_easy_setopt(c, CURLOPT_URL, req.c_str());
    e = curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, callback_book);
    e = curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)sec_id);
    while(can_run) {
        mlog() << "init_book";
        e = curl_easy_perform(c);
        mlog() << "sleep(" << config::instance().sleep_time << ")";
        usleep(config::instance().sleep_time * 1000);
    }
}

void init_trades(uint32_t sec_id, const std::string& symbol, volatile bool& can_run)
{
    const config& cfg = config::instance();
    //https://api.bitfinex.com/v2/trades/tBTCUSD
    std::string req = api + "/trades/t" + symbol + "/"  + cfg.precision;
    mlog() << "curl_req: " << req;
    parser::security& s = parser::instance().secs[sec_id];
    CURL* c = s.ct.get();
    e = curl_easy_setopt(c, CURLOPT_URL, req.c_str());
    e = curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, callback_trades);
    e = curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)sec_id);
    while(can_run) {
        e = curl_easy_perform(c);
        mlog() << "sleep(" << config::instance().sleep_time << ")";
        usleep(config::instance().sleep_time * 1000);
    }
}

void proceed_bitfinex(volatile bool& can_run)
{
    binary_dumper bd;
    const config& cfg = config::instance();
    if(cfg.curl_log)
        bd.init(cfg.curl_logname);
    while(can_run) {
        try {
            parser p;
            for(auto symbol: cfg.symbols) {
                uint32_t sec_id = parser::instance().init(symbol);
                if(cfg.book)
                    init_book(sec_id, symbol, can_run);
                //if(cfg.trades)
                //    init_trades(sec_id, symbol, can_run);
            }
        }
        catch(std::exception& e) {
            mlog() << "proceed_bitfinex " << e;
            if(can_run)
                usleep(5000 * 1000);
        }
    }
}

