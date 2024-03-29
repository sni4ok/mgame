/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mirror.hpp"
#include "ncurses.hpp"

#include "../makoa/types.hpp"

#include "../evie/thread.hpp"
#include "../evie/queue.hpp"

#include <unistd.h>
#include <stdlib.h>

ncurses_err e;

struct security_filter
{
    mstring security;
    uint32_t security_id;

    security_filter(const mstring& s) : security(s), security_id() {
        uint32_t sid = atoi(s.c_str());
        mstring sec = to_string(sid);
        if(sec == s)
            security_id = sid;
    }
    bool equal(const message& m) {
        if(m.id == msg_book)
            return m.mb.security_id == security_id;
        if(m.id == msg_trade)
            return m.mt.security_id == security_id;
        if(m.id == msg_clean)
            return m.mc.security_id == security_id;
        if(m.id == msg_instr) {
            if(!security_id && (security == from_array(m.mi.security) || security == "$0"))
                security_id = m.mi.security_id;
            return m.mi.security_id == security_id;
        }
        throw str_exception("security_filter() unsupported msg type");
    }
};

char get_direction(uint32_t direction)
{
    if(direction == 0)
        return 'U';
    if(direction == 1)
        return 'B';
    if(direction == 2)
        return 'S';
    throw mexception(es() % "bad direction: " % direction);
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
    if(minus)
        s << "-";
    s << (v.value / ttime_t::frac) << "." << mlog_fixed<9>(v.value % ttime_t::frac);
    return s;
}

struct mirror::impl
{
    security_filter sec;
    uint32_t refresh_rate;
    
    order_book ob;
    queue<message_trade> trades;
    message_instr mi;
    mstring head_msg, head_msg_auto_scroll;

    bool auto_scroll;
    price_t top_order_p, auto_scroll_p;
    uint32_t trades_from, trades_width_limit;

    char buf[512];
    buf_stream bs;
 
    int64_t dE, dP;
    
    volatile bool can_run;
    my_mutex mutex;
    thread refresh_thrd;

    const char empty[10];
    impl(const mstring& sec, uint32_t refresh_rate_ms) :
        sec(sec), refresh_rate(refresh_rate_ms * 1000),
        auto_scroll(true), top_order_p(), auto_scroll_p(), trades_from(), trades_width_limit(), bs(buf, buf + sizeof(buf) - 1),
        dE(), dP(), can_run(true), refresh_thrd(&impl::refresh_thread, this),
        empty("         ")
    {
    }
    ~impl()
    {
        can_run = false;
        refresh_thrd.join();
    }
    void set_auto_scroll()
    {
        head_msg_auto_scroll = head_msg + (auto_scroll ? str_holder("     auto_scroll on ") : str_holder("     auto_scroll off"));
    }
    void set_instrument(const message_instr& i)
    {
        mi = i;
        head_msg = mstring(from_array(mi.exchange_id)) + "/" + from_array(mi.feed_id) + "/" + from_array(mi.security);
        set_auto_scroll();
    }
    order_book::price_iterator get_top_order(window& w)
    {
        if(auto_scroll)
        {
            order_book::price_iterator ib = ob.orders_p.begin(), ie = ob.orders_p.end(), it;
            it = auto_scroll_p.value ? ob.orders_p.lower_bound(auto_scroll_p) : ib;
            while(it != ie && it->second.count.value >= 0)
                ++it;
            uint32_t count = 0;
            while(it != ib)
            {
                --it;
                if(it->second.count.value > 0)
                    ++count;
                if(count == w.rows / 2)
                    return it;
            }
            return ib;
        }

        if(!top_order_p.value)
            return ob.orders_p.begin();

        auto it = ob.orders_p.lower_bound(top_order_p);
        auto ib = ob.orders_p.begin(), ie = ob.orders_p.end();
        while(it != ie && it != ib && !it->second.count.value)
            --it;
        if(it != ie)
            top_order_p = it->first;
        
        return it;
    }
    void print_trades(window& w)
    {
        if(w.rows <= trades.size())
            trades.pop_front(trades.size() + 1 - w.rows);
        uint32_t i = 0;
        if(trades_from == 0)
        {
            trades_from = 48;
            trades_width_limit = w.cols - trades_from;
        }
        for(auto&& v : trades)
        {
            bs << brief_time(v.etime) << " " << brief_time(v.time) << " " << v.price << " " << v.count << " " << get_direction(v.direction);
            if(bs.size() > trades_width_limit)
                bs.resize(trades_width_limit);
            bs << '\0';
            e = mvwaddstr(w, i++, trades_from, bs.begin());
            bs.clear();
        }
    }
    void print_order_book(window& w)
    {
        if(!sec.security_id)
            return;
        order_book::price_iterator it = get_top_order(w), ie = ob.orders_p.end();
        e = attron(A_BOLD);
        for(uint32_t i = 0; i != w.rows - 1; ++i, ++it)
        {
            while((it != ie) && !it->second.count.value)
                ++it;
            if(it == ie)
                break;

            e = attron(COLOR_PAIR(it->second.count.value < 0 ? 3 : 2));
            bs << brief_time(it->second.time) << " " << it->first << " " << it->second.count;
            if(trades_from > bs.size() + 5)
                bs.write(empty, trades_from - bs.size() - 5);
            bs << '\0';
            e = mvwaddstr(w, i, 0, bs.begin());
            trades_from = std::max<uint32_t>(trades_from, bs.size() + 4);
            trades_width_limit = w.cols - trades_from;
            bs.clear();
        }
        e = attroff(A_BOLD);
        e = attron(COLOR_PAIR(1));
    }
    void print_head(window& w)
    {
        e = mvwaddstr(w, w.rows - 1, 0, head_msg_auto_scroll.c_str());
        bs << "dE: " << print_dt(dE) << ", dP: " << print_dt(dP) << '\0';
        e = mvwaddstr(w, w.rows - 1, w.cols - bs.size(), bs.begin());
        bs.clear();
    }
    void refresh(window& w)
    {
        my_mutex::scoped_lock lock(mutex);
        w.clear();
        print_order_book(w);
        print_trades(w);
        print_head(w);
        lock.unlock();
        move(w.rows - 1, 0);
        ::refresh();
    }
    void wait_input(window& w)
    {
        uint32_t c = refresh_rate / 20000;
        int key = -1;
        for(uint32_t i = 0; i != c; ++i)
        {
            key = getch();
            if(key == -1)
                usleep(20000);
            else
            {
                //mlog() << "key: " << key;
                if(key == 97) // 'a'
                {
                    auto_scroll = !auto_scroll;
                    set_auto_scroll();
                }
                else if(key == 259 || key == 339) // arrow up and page up
                {
                    uint32_t r = (key == 259 ? 1 : w.rows);
                    my_mutex::scoped_lock lock(mutex);
                    order_book::price_iterator it = get_top_order(w), ib = ob.orders_p.begin();

                    for(uint32_t i = 0; i != r; ++i) {
                        if(it != ib) {
                            --it;
                            while(it != ib && !it->second.count.value)
                                --it;
                        }
                    }
                    top_order_p = it->first;
                }
                else if(key == 258 || key == 338) //arrow down or page down
                {
                    uint32_t r = (key == 258 ? 0 : w.rows - 1);
                    my_mutex::scoped_lock lock(mutex);
                    order_book::price_iterator it = ob.orders_p.upper_bound(top_order_p), ie = ob.orders_p.end();

                    auto ib = it;
                    while(it != ie && !it->second.count.value)
                        ++it;
                    for(uint32_t i = 0; i != r; ++i) {
                        if(it != ie) {
                            ++it;
                            while(it != ie && !it->second.count.value)
                                ++it;
                        }
                    }
                    if(it == ie && ib != ie)
                        --it;

                    if(it != ie)
                        top_order_p = it->first;
                }
                break;
            }
        }
    }
    void refresh_thread()
    {
        try {
            window w;
            e = start_color()
                & init_pair(1, COLOR_WHITE, COLOR_BLACK)
                & init_pair(2, COLOR_BLUE, COLOR_YELLOW)
                & init_pair(3, COLOR_BLUE, COLOR_CYAN);

            while(can_run) {
                refresh(w);
                wait_input(w);
            }
        }
        catch(std::exception& e) {
            mlog() << "mirror::refresh_thread() " << e;
        }
    }
    void proceed(const message& m)
    {
        if(m.id == msg_ping)
            return;
        if(sec.equal(m)) {
            my_mutex::scoped_lock lock(mutex);
            if(m.id == msg_instr)
                set_instrument(m.mi);
            if(m.id == msg_trade) {
                trades.push_back(m.mt);
                ttime_t ct = cur_ttime();
                dE = ct - m.mt.etime;
                dP = ct - m.mt.time;
            }
            else {
                ob.proceed(m);
                if(m.id == msg_book)
                    dP = cur_ttime() - m.mb.time;
            }
        }
    }
    void proceed(const message* m, uint32_t count)
    {
        for(uint32_t i = 0; i != count; ++i, ++m)
            proceed(*m);
    }
};

inline mirror::impl* create_mirror(const mstring& params)
{
    auto p = split(params, ' ');
    if(p.size() > 2)
        throw mexception(es() % "mirror::mirror() bad params: " % params);
    uint32_t refresh_rate = p.size() == 2 ? lexical_cast<uint32_t>(p[1]) : 100;
    if(!refresh_rate)
        throw mexception(es() % "mirror::mirror() bad params: " % params % ", refresh_rate should be at least 1");
    return new mirror::impl(p[0], refresh_rate);
}

mirror::mirror(const mstring& params)
{
    pimpl = create_mirror(params);
}

mirror::~mirror()
{
    delete pimpl;
}

void mirror::proceed(const message* m, uint32_t count)
{
    pimpl->proceed(m, count);
}

void* ying_init(const char* params)
{
    mlog() << "ying " << _str_holder(params) << " started";
    return create_mirror(_mstring(params));
}

void ying_destroy(void* w)
{
    delete ((mirror::impl*)(w));
}

void ying_proceed(void* w, const message* m, uint32_t count)
{
    ((mirror::impl*)(w))->proceed(m, count);
}

