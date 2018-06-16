/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mirror.hpp"
#include "ncurses.hpp"

#include "evie/utils.hpp"

#include <deque>
#include <mutex>
#include <thread>

#include <unistd.h>

ncurses_err e;

bool is_security_equal(const message& m, uint32_t security_id)
{
    if(m.id == msg_book)
        return m.mb.security_id == security_id;
    if(m.id == msg_trade)
        return m.mt.security_id == security_id;
    if(m.id == msg_clean)
        return m.mc.security_id == security_id;
    if(m.id == msg_instr)
        return m.mi.security_id == security_id;

    throw std::logic_error("is_security_equal() unsupported msg type");
}

char get_direction(uint32_t direction)
{
    if(direction == 0)
        return 'U';
    if(direction == 1)
        return 'B';
    if(direction == 2)
        return 'S';
    throw std::runtime_error(es() % "bad direction: " % direction);
}

int64_t operator-(ttime_t l, ttime_t r)
{
    if(l.value > r.value)
        return int64_t(l.value - r.value);
    else
        return -int64_t(r.value - l.value);
}

struct print_dt
{
    int64_t value;
    explicit print_dt(int64_t value) :  value(value)
    {
    }
};
template<typename stream>
stream& operator<<(stream& s, print_dt v)
{
    bool minus = false;
    if(v.value < 0) {
        minus = true;
        v.value = -v.value;
    }
    s << (minus ? "-" : "") << (v.value / 1000000) << "." << mlog_fixed<6>(v.value % 1000000);
    return s;
}

struct mirror::impl
{
    const uint32_t security_id, refresh_rate;
    
    order_books ob;
    std::deque<message_trade> trades;
    message_instr mi;
    std::string head_msg;
    uint32_t orders_sz;

    window w;
  
    std::stringstream s;
    uint32_t trades_from;

    int64_t dE, dP;

    void print_trades()
    {
        while(w.cols <= trades.size())
            trades.pop_front();
        uint32_t i = 0;
        for(auto&& v : trades)
        {
            s.str("");
            s << brief_time(v.etime) << " " << brief_time(v.time) << " " << v.price << " " << v.count << " " << get_direction(v.direction);
            e = mvwaddstr(w, i++, trades_from, s.str().c_str());
        }
    }

    void set_instrument(const message_instr& i)
    {
        mi = i;
        head_msg = std::string(mi.exchange_id) + "/" + mi.feed_id + "/" + mi.security + " " + std::to_string(orders_sz);
    }
    
    void print_pips(const char* ib, uint32_t sz, uint32_t width)
    {
        const char *ie = ib + sz;
        for(uint32_t i = 0; ib != ie; ++i)
        {
            auto ii = std::find(ib, ie, '\n');
            e = mvwaddnstr(w, i, w.cols - width, ib, ii - ib);
            if(ii != ie)
                ++ii;
            ib = ii;
        }
    }
    
    volatile bool can_run;
    std::mutex mutex;
    std::thread refresh_thrd;
    impl(uint32_t security_id, uint32_t refresh_rate_ms) :
        security_id(security_id), refresh_rate(refresh_rate_ms * 1000),
        orders_sz(), dE(), dP(), can_run(true), refresh_thrd(&impl::refresh_thread, this)
    {
    }

    ~impl()
    {
        can_run = false;
        refresh_thrd.join();
    }

    void print_order_book()
    {
        ob.compact();
        auto& v = ob.get_orders(security_id);
        orders_sz = v.size();
        //we reject levels that not fit to our window
        //TODO: rework for keyboard manipulation
        auto it = v.begin(), ie = v.end();
        for(uint32_t i = 0; i != w.rows - 1 && it != ie; ++i, ++it)
        {
            s.str("");
            s << brief_time(it->second.time) << " " << it->first << " " << it->second.count;
            std::string v = s.str();
            trades_from = std::max<uint32_t>(trades_from, v.size() + 4);
            e = mvwaddstr(w, i, 0, v.c_str());
        }
    }
    
    void print_head()
    {
        e = mvwaddnstr(w, w.rows - 1, 0, &w.blank_row[0], w.blank_row.size() - 1);
        e = mvwaddstr(w, w.rows - 1, 0, head_msg.c_str());
        std::stringstream str;
        str << "dE: " << print_dt(dE) << ", dP: " << print_dt(dP);
        std::string s = str.str();
        e = mvwaddstr(w, w.rows - 1, w.cols - 1 - s.size(), s.c_str());
    }

    void refresh()
    {
        std::unique_lock<std::mutex> lock(mutex);
        w.clear();
        print_order_book();
        print_trades();
        print_head();
        lock.unlock();
        move(w.rows - 1, 0);
        ::refresh();
    }
    void refresh_thread()
    {
        try {
            while(can_run) {
                refresh();
                usleep(refresh_rate);
            }
        }
        catch(std::exception& e) {
            mlog() << "mirror_refresh_thread " << e;
        }
    }

    void proceed(const message& m)
    {
        if(m.id == msg_ping)
            return;
        if(is_security_equal(m, security_id)) {
            std::unique_lock<std::mutex> lock(mutex);
            if(m.id == msg_instr)
                set_instrument(m.mi);
            if(m.id == msg_trade) {
                trades.push_back(m.mt);
                ttime_t ct = get_cur_ttime();
                dE = ct - m.mt.etime;
                dP = ct - m.mt.time;
            }
            else {
                ob.proceed(m);
                if(m.id == msg_book)
                    dP = get_cur_ttime() - m.mb.time;
            }
        }
    }
};

mirror::mirror(uint32_t security_id, uint32_t refresh_rate)
{
    pimpl = std::make_unique<impl>(security_id, refresh_rate);
}

mirror::mirror(const std::string& params)
{
    auto p = split(params, ' ');
    if(p.size() != 2)
        throw std::runtime_error(es() % "mirror::mirror() bad params: " % params);
    uint32_t security_id = atoi(p[0].c_str());
    uint32_t refresh_rate = atoi(p[1].c_str());
    if(!refresh_rate)
        throw std::runtime_error(es() % "mirror::mirror() bad params: " % params % ", refresh_rate should be at least 1");
    pimpl = std::make_unique<impl>(security_id, refresh_rate);
}

mirror::~mirror()
{
}

void mirror::proceed(const message& m)
{
    pimpl->proceed(m);
}
    
void mirror::refresh()
{
    pimpl->refresh();
}

