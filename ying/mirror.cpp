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

uint32_t get_security_id(const message& m)
{
    if(m.id == msg_book)
        return m.mb.security_id;
    if(m.id == msg_trade)
        return m.mt.security_id;
    if(m.id == msg_clean)
        return m.mc.security_id;
    if(m.id == msg_instr)
        return m.mi.security_id;
    throw str_exception("get_security_id unsupported type");
}

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
    bool equal(const message& m, uint32_t sec_id) {
        if(security_id == sec_id)
            return true;
        if(m.id == msg_instr) {
            if(!security_id && (security == from_array(m.mi.security) || security == "$0"))
                security_id = m.mi.security_id;
            return m.mi.security_id == security_id;
        }
        return false;
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
    fmap<uint32_t, message_instr> all_securities;
    std::unordered_map<uint32_t, pair<order_book_ba, queue<message_trade> > > all_books;

    security_filter sec;
    uint32_t refresh_rate;
    
    order_book_ba* ob;

    struct book
    {
        count_t count;
        ttime_t time;
        price_t price;

        bool operator==(const book& b) const {
            return count.value == b.count.value && time.value == b.time.value && price.value == b.price.value;
        }
        bool operator!=(const book& b) const {
            return !(*this == b);
        }
    };
    mvector<book> books_printed;

    queue<message_trade>* trades;
    message_instr mi;
    mstring head_msg, head_msg_auto_scroll;
    bool head_msg_auto_scroll_printed = false;

    bool auto_scroll;
    price_t top_order_p;
    uint32_t trades_from = 48, trades_width = 0;
    book last_printed_trade;

    my_stream bs;
 
    int64_t dE, dP;
    bool dEdP_printed = false;
    
    volatile bool can_run;
    my_mutex mutex;
    jthread refresh_thrd;

    const char empty[10];

    impl(const mstring& sec, uint32_t refresh_rate_ms) :
        sec(sec), refresh_rate(refresh_rate_ms * 1000), ob(),
        auto_scroll(true), top_order_p(), trades_from(),
        dE(), dP(), can_run(true), refresh_thrd(&impl::refresh_thread, this),
        empty("         ")
    {
    }
    ~impl()
    {
        can_run = false;
    }
    void set_auto_scroll()
    {
        head_msg_auto_scroll = head_msg + (auto_scroll ? str_holder("     auto_scroll on ") : str_holder("     auto_scroll off"));
        head_msg_auto_scroll_printed = false;
    }
    void set_instrument(const message_instr& i)
    {
        mi = i;
        head_msg = mstring(from_array(mi.exchange_id)) + "/" + from_array(mi.feed_id) + "/" + from_array(mi.security);
        set_auto_scroll();
    }
    int32_t get_top_order(window& w)
    {
        if(auto_scroll || !top_order_p.value)
            return -min<int32_t>(ob->bids.size(), w.rows / 2);
        {
            if(!ob->bids.empty() && top_order_p.value <= ob->bids.begin()->first.value)
            {
                auto it = ob->bids.lower_bound(top_order_p);
                return - int32_t((it - ob->bids.begin()));
            }
        }
        {
            auto it = ob->asks.lower_bound(top_order_p);
            return it - ob->asks.begin();
        }
    }
    void print_trades(window& w)
    {
        if(w.rows <= trades->size())
            trades->pop_front(trades->size() + 1 - w.rows);

        auto& v = *trades->rbegin();
        book tr{v.count, v.time, v.price};

        if(tr == last_printed_trade)
            return;

        uint32_t i = 0;
        uint32_t trades_width_limit = w.cols - trades_from;
        for(auto&& v : *trades)
        {
            bs << "     " << brief_time(v.etime) << " " << brief_time(v.time) << " " << v.price << " " << v.count << " "
                << get_direction(v.direction);
            if(bs.size() < trades_width)
                bs.write(w.blank_row.begin(), trades_width - bs.size());
            else if(bs.size() > trades_width_limit)
                bs.resize(trades_width_limit);
            else
                trades_width = max<uint32_t>(trades_width, bs.size());
            e = mvwaddnstr(w, i++, trades_from, bs.begin(), bs.size());
            bs.clear();
        }
        for(; i != w.rows - 1; ++i)
            e = mvwaddnstr(w, i, trades_from, w.blank_row.begin(), w.cols - trades_from);
        last_printed_trade = std::move(tr);
    }
    void print_book(bool bids, price_t price, const order_book_ba::book& b, window& w, uint32_t& row)
    {
        count_t c(bids ? b.count.value : - b.count.value);
        book bo{c, b.time, price};
        if(books_printed[row] != bo)
        {
            e = attron(COLOR_PAIR(bids ? 2 : 3));
            bs << brief_time(b.time) << " " << price << " " << c;
            if(trades_from > bs.size())
                bs.write(empty, trades_from - bs.size());
            e = mvwaddnstr(w, row, 0, bs.begin(), bs.size());
            if(bs.size() > trades_from)
            {
                last_printed_trade = book();
                trades_from = bs.size();
            }
            bs.clear();
            books_printed[row] = bo;
        }
        ++row;
    }
    void print_order_book(window& w)
    {
        int32_t it = get_top_order(w);
        e = attron(A_BOLD);
        uint32_t row = 0;
        if(it < 0)
        {
            auto b = ob->bids.begin() - it, i = ob->bids.begin();
            for(; row != w.rows - 1 && b >= i; --b)
                print_book(true, b->first, b->second, w, row);
        }
        {
            auto b = ob->asks.begin(), i = ob->asks.end();
            for(; row != w.rows - 1 && b != i; ++b)
                print_book(false, b->first, b->second, w, row);
        }
        e = attroff(A_BOLD);
        e = attron(COLOR_PAIR(1));
    }
    void print_head(window& w)
    {
        if(!head_msg_auto_scroll_printed)
        {
            e = mvwaddnstr(w, w.rows - 1, 0, head_msg_auto_scroll.begin(), head_msg_auto_scroll.size());
            head_msg_auto_scroll_printed = true;
        }
        if(!dEdP_printed)
        {
            bs << "dE: " << print_dt(dE) << ", dP: " << print_dt(dP) << '\0';
            //e = mvwaddstr(w, w.rows - 1, head_msg_auto_scroll.size(), w.blank_row.begin() + head_msg_auto_scroll.size() + bs.size());
            e = mvwaddnstr(w, w.rows - 1, w.cols - bs.size(), bs.begin(), bs.size());
            bs.clear();
            dEdP_printed = true;
        }
    }
    void refresh(window& w)
    {
        books_printed.resize(w.rows);
        my_mutex::scoped_lock lock(mutex);
        if(!sec.security_id)
            return;
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
                my_mutex::scoped_lock lock(mutex);
                //mlog() << "key: " << key;
                if(key == 97) // 'a'
                {
                    auto_scroll = !auto_scroll;
                    set_auto_scroll();
                }
                else if(key == 260 || key == 261) //arrow left || arrow right
                {
                    if(!all_securities.empty() && sec.security_id) {
                        auto i = all_securities.find(sec.security_id);
                        if(i != all_securities.end()) {
                            if(key == 260)
                            {
                                if(i == all_securities.begin())
                                    i = all_securities.end() - 1;
                                else
                                    --i;
                            }
                            else
                            {
                                ++i;
                                if(i == all_securities.end())
                                    i = all_securities.begin();
                            }
                            auto& v = all_books[i->first];
                            ob = &v.first;
                            trades = &v.second;
                            sec.security_id = i->first;
                            top_order_p = price_t();
                            set_instrument(i->second);
                        }
                    }
                }
                else if(key == 259 || key == 339 || key == 258 || key == 338) // arrow up or page up or arrow down or page down
                {
                    int32_t it = get_top_order(w);
                    if(key == 259)
                        --it;
                    else if(key == 339)
                        it -= w.rows - 1;
                    else if(key == 258)
                        ++it;
                    else
                        it += w.rows - 1;
                    if(it <= 0) {
                        if(ob->bids.empty())
                            top_order_p = price_t();
                        else {
                            if(uint32_t(-it) >= ob->bids.size())
                                top_order_p = ob->bids.rbegin()->first;
                            else
                                top_order_p = (ob->bids.begin() - it)->first;
                        }
                    }
                    else {
                        if(ob->asks.empty())
                            top_order_p = price_t();
                        else {
                            if(uint32_t(it) >= ob->asks.size())
                                top_order_p = ob->asks.rbegin()->first;
                            else
                                top_order_p = (ob->asks.begin() + it)->first;
                        }
                    }
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
        catch(exception& e) {
            mlog() << "mirror::refresh_thread() " << e;
        }
    }
    void proceed(const message& m)
    {
        if(m.id == msg_ping)
            return;
        uint32_t security_id = get_security_id(m);
        my_mutex::scoped_lock lock(mutex);
        if(m.id == msg_instr)
            all_securities[security_id] = m.mi;
        if(m.id != msg_trade)
            all_books[security_id].first.proceed(m);
        else
            all_books[security_id].second.push_back(m.mt);

        if(sec.equal(m, security_id)) {
            if(!ob) {
                auto& v = all_books[security_id];
                ob = &v.first;
                trades = &v.second;
            }
            if(m.id == msg_instr)
                set_instrument(m.mi);
            if(m.id == msg_trade) {
                ttime_t ct = cur_ttime();
                dE = ct - m.mt.etime;
                dP = ct - m.mt.time;
                dEdP_printed = false;
            }
            else {
                if(m.id == msg_book) {
                    dP = cur_ttime() - m.mb.time;
                    dEdP_printed = false;
                }
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
    auto p = split(params.str(), ' ');
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

