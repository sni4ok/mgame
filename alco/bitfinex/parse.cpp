/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "parse.hpp"
#include "config.hpp"

#include "makoa/order_book.hpp"
#include "viktor/viktor.hpp"
#include "tyra/tyra.hpp"
#include "evie/curl.hpp"

#include "memory"

#include <unistd.h>

struct binary_dumper : stack_singleton<binary_dumper>
{
    std::ofstream s;
    binary_dumper()
    {
    }
    void init(const std::string& fname)
    {
        if(!s) {
            s.open(fname, std::ofstream::out | std::ofstream::trunc);
            if(!s)
                throw std::runtime_error(es() % "open file \"" % fname % "\" error");
        }
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
    //todo: rewrite it without atof
    return {atof(f) * (1. + std::numeric_limits<double>::epsilon()) * double(price_frac)};
}

count_t get_count(const char* f, const char* t)
{
    return {atof(f) * (1. + std::numeric_limits<double>::epsilon()) * double(count_frac)};
}

int32_t get_amount(const char* f, const char* t)
{
    return atoi(f);
}

struct parser : stack_singleton<parser>
{
    void* v;

    struct security
    {
        message mb;
        message mt;
        message mc;
        message mi;
        
        char tail_book[128], tail_trades[128];
        size_t tail_book_sz, tail_trades_sz;

        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> cb, ct;
        std::map<price_t, count_t> old_ob, cur_ob;

        security() : tail_book_sz(), tail_trades_sz(), cb(curl_easy_init(), &curl_easy_cleanup),
            ct(curl_easy_init(), &curl_easy_cleanup)
        {
        }
        void proceed_book(price_t p, count_t c, bool flush)
        {
            tyra_msg<message_book>& tb = (tyra_msg<message_book>&)mb;
            tb.price = p;
            tb.count = c;
            tb.time = get_cur_ttime();
            mb.flush = flush;
            viktor_proceed(mb);
        }
        void push_flush()
        {
            if(cur_ob.empty())
                return;
            for(auto& v: old_ob)
                v.second.value = - v.second.value;
            for(const auto& v: cur_ob)
                old_ob[v.first].value = v.second.value;

            uint32_t sz = old_ob.size();
            mlog() << "sz:" << sz;
            if(sz) {
                for(auto& v: old_ob) {
                    proceed_book(v.first, v.second, !(--sz));
                }
            }
            old_ob.clear();
            old_ob.swap(cur_ob);
        }
    };
    std::vector<security> secs;

    parser()
    {
        v = viktor_init(config::instance().push);
    }

    uint32_t init(const std::string& ticker)
    {
        security sec;
        tyra_msg<message_book>& mb = (tyra_msg<message_book>&)sec.mb;
        mb = tyra_msg<message_book>();

        tyra_msg<message_trade>& mt = (tyra_msg<message_trade>&)sec.mt;
        mt = tyra_msg<message_trade>();

        tyra_msg<message_clean>& mc = (tyra_msg<message_clean>&)sec.mc;
        mc = tyra_msg<message_clean>();

        tyra_msg<message_instr>& mi = (tyra_msg<message_instr>&)sec.mi;
        mi = tyra_msg<message_instr>();

        message_instr& m = mi;
        
        memset(&m.exchange_id, 0, sizeof(m.exchange_id));
        memset(&m.feed_id, 0, sizeof(m.feed_id));
        memset(&m.security, 0, sizeof(m.security));

        strcpy(m.exchange_id, "bitfinex");
        strcpy(m.feed_id, config::instance().feed.c_str());
        strcpy(m.security, ticker.c_str());

        crc32 crc(0);
        crc.process_bytes((const char*)(&m), sizeof(message_instr) - 12);
        m.security_id = crc.checksum();

        mb.security_id = m.security_id;
        mt.security_id = m.security_id;
        mc.security_id = m.security_id;

        mi.time = get_cur_ttime();
        viktor_proceed(sec.mi);

        secs.push_back(std::move(sec));
        return secs.size() - 1;
    }

    void proceed_book(uint32_t sec_id, price_t p, count_t c, bool flush)
    {
        security& sec = secs[sec_id];
        sec.proceed_book(p, c, flush);
    }

    ~parser()
    {
        viktor_destroy(v);
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
    int32_t a = 0;
    count_t c;
    bool flush;
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
        
        if((ie - z3) > 30)
            flush = false;
        else
            flush = std::find(z3, ie, ']') == ie;
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

    auto& ob = sec.cur_ob;

    size_t size = sz * nmemb;
    char *ptr = (char*)contents, *ie = ptr + size;
    
    if(config::instance().curl_log) {
        mlog() << "proceed_book: " << sz << "x" << nmemb << ", tail_sz: " << tail_sz << ", tail: " << std::string(tail, tail + tail_sz);
        binary_dumper::instance().write(ptr, size);
    }

    if(tail_sz) {
        auto p = std::find(ptr, ie, ']');
        if(p == ie || (p - ptr) > (sizeof(tail) - 1 - tail_sz))
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
        e = curl_easy_perform(c);
        usleep(config::instance().sleep_time * 1000);
        mlog() << "sleep";
    }
}

struct parse_pp
{
    //[[a,b,c,d],[a,b,c,d]]
    char tail[128];
    size_t tail_sz;
    
    char *it, *ie;

    void set(char *ptr, size_t size)
    {
    }
    
    template<typename type>
    bool get(type& v)
    {
        return false;
        //char* i = std::find(it, ie, ']');
    }

    //template<typename type, typename ... types>
    //bool get()
    //{
    //}

    bool is_end()
    {
    }
};

size_t callback_trades(void *contents, size_t sz, size_t nmemb, void* p)
{
    uint32_t sec_id = get_sec_id(p);
    auto& sec = (parser::instance().secs)[sec_id];
    auto& tail = sec.tail_trades;
    auto& tail_sz = sec.tail_trades_sz;
    
    size_t size = sz * nmemb;
    char *ptr = (char*)contents, *ie = ptr + size;
    
    if(config::instance().curl_log) {
        mlog() << "proceed_trade: " << sz << "x" << nmemb << ", tail_sz: " << tail_sz << ", tail: " << std::string(tail, tail + tail_sz);
        binary_dumper::instance().write(ptr, size);
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
    e = curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, callback_book);
    e = curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)sec_id);
    while(can_run) {
        e = curl_easy_perform(c);
        usleep(config::instance().sleep_time * 1000);
        mlog() << "sleep";
    }
}

void proceed_bitfinex(volatile bool& can_run)
{
    binary_dumper bd;
    const config& cfg = config::instance();
    if(cfg.curl_log)
        bd.init(cfg.curl_logname);
    parser p;
    for(auto symbol: cfg.symbols) {
        uint32_t sec_id = parser::instance().init(symbol);
        if(cfg.book)
            init_book(sec_id, symbol, can_run);
        if(cfg.trades)
            init_trades(sec_id, symbol, can_run);
    }
}

