/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mirror.hpp"
#include "ncurses.hpp"

#include "../makoa/types.hpp"

#include "../evie/thread.hpp"
#include "../evie/queue.hpp"
#include "../evie/math.hpp"

#include <unordered_map>
#include <stdlib.h>

static ncurses_err e;

u32 get_security_id(const message& m)
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
    u32 security_id;

    security_filter(const mstring& s) : security(s), security_id()
    {
        u32 sid = atoi(s.c_str());
        mstring sec = to_string(sid);
        if(sec == s)
            security_id = sid;
    }
    bool equal(const message& m, u32 sec_id)
    {
        if(security_id == sec_id)
            return true;
        if(m.id == msg_instr)
        {
            if(!security_id && from_any(security, from_array(m.mi.security), "$0"))
                security_id = m.mi.security_id;
            return m.mi.security_id == security_id;
        }
        return false;
    }
};

char get_direction(u32 direction)
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
    i64 value;

    explicit print_dt(i64 value) : value(value)
    {
    }
};

template<typename stream>
stream& operator<<(stream& s, print_dt v)
{
    bool minus = false;
    if(v.value < 0)
    {
        minus = true;
        v.value = -v.value;
    }
    if(minus)
        s << "-";
    s << (v.value / frac<ttime_t>()) << "." << uint_fixed<9>(v.value % frac<ttime_t>());
    return s;
}

void wstr(WINDOW* w, int y, int x, char_cit str, int n)
{
    e = mvwaddnstr(w, y, x, str, n);
}

template<typename type>
struct decimal_fixed
{
    type min_v, max_v;
    u32 digits;

    decimal_fixed() : min_v(limits<type>::max), max_v(limits<type>::min), digits()
    {
    }

    void operator()(auto& s, type v, u32 decimal)
    {
        if(v < min_v)
            min_v = v;

        if(v > max_v)
            max_v = v;

        type p = abs(v);

        u64 m = p.value / frac<type>();
        u32 d = log10(m);

        if(v < type())
            ++d;

        if(digits < d)
            digits = d;

        if(v < type())
            s << "-";
        if(digits)
            s << uint_fix{m, digits, false};
        else
            s << "0";

        if(decimal)
        {
            u64 v = u64(p.value) % frac<type>();
            ASSERT(decimal <= -type::exponent);
            v /= __pow10[-type::exponent - decimal];
            s << "." << uint_fix{v, decimal, true};
        }
    }
};

struct decimal_params
{
    u32 price_decimal = 0;
    u32 count_decimal = 0;

    u64 price_frac = __pow10[-price_t::exponent - price_decimal];
    u64 count_frac = __pow10[-count_t::exponent - count_decimal];

    template<typename type>
    static void set(u32& decimal, u64& frac, type t)
    {
        u64 p;
        p = abs(t.value) % ::frac<type>();
        p = p % frac;

        if(p)
        {
            while(!(p % 10))
                p /= 10;

            u32 d = log10(p);
            decimal += d;
            frac = __pow10[-type::exponent - decimal];
        }
    }
    void set(price_t price, count_t count)
    {
        set(price_decimal, price_frac, price);
        set(count_decimal, count_frac, count);
    }
};

struct time_params
{
    bool print_time = true;
    int mode = print_td::ms;
    bool print_count = true;

    u32 time_size(bool pt) const
    {
        if(!pt)
            return 0;

        mstream s;
        s << print_td(cur_ttime(), mode) << " ";
        return s.size();
    }
    u32 view_size(const decimal_params& p) const
    {
        mstream s;
        decimal_fixed<price_t> pt;
        pt(s, lexical_cast<price_t>("2e4"), p.price_decimal);
        if(print_count)
        {
            s << " ";
            decimal_fixed<count_t> c;
            c(s, lexical_cast<count_t>("2e4"), p.count_decimal);
        }
        return s.size() + time_size(print_time);
    }
    void serialize(buf_stream& s) const
    {
        s << "t" << print_time << "m" << mode << "c" << print_count;
    }
    char_cit load(char_cit f, char_cit t, char last = char())
    {
        auto err = [&]()
        {
            throw mexception(es() % "mirror_params error: " % str_holder(f, t));
        };

        if(f == t || *f != 't')
            err();

        char_cit it = f + 1, ne;
        ne = find(f, t, 'm');
        if(ne == t)
            err();

        print_time = lexical_cast<bool>(it, ne);
        it = ne + 1;
        ne = find(f, t, 'c');
        if(ne == t)
            err();

        mode = lexical_cast<int>(it, ne);
        if(mode < print_td::sec || mode > print_td::ns)
            err();

        it = ne + 1;
        ne = last ? find(f, t, last) : t;
        print_count = lexical_cast<bool>(it, ne);

        return ne;
    }
};

struct glass_params : time_params
{
};

struct trades_params : time_params
{
    bool print_etime = true;

    void serialize(buf_stream& s) const
    {
        time_params::serialize(s);
        s << "e" << print_etime;
    }
    void load(char_cit f, char_cit t)
    {
        char_cit it = time_params::load(f, t, 'e');
        if(it == t)
            throw str_exception("mirror_params, trades_params error");

        ++it;
        print_etime = lexical_cast<bool>(it, t);
    }
};

struct mirror::impl
{
    fmap<u32, message_instr> all_securities;
    std::unordered_map<u32, pair<order_book_ba, queue<message_trade> > > all_books;

    decimal_params dp;
    glass_params gp;
    trades_params tp;

    security_filter sec;
    u32 refresh_rate;
    
    order_book_ba* ob;

    struct book
    {
        count_t count;
        ttime_t time;
        price_t price;

        bool operator==(const book& b) const
        {
            return count == b.count && time == b.time
                && price == b.price;
        }
    };

    mvector<book> books_printed;
    queue<message_trade>* trades;
    message_instr mi;
    mstring head_msg, head_msg_auto_scroll;
    bool head_msg_auto_scroll_printed = false;

    bool auto_scroll;
    price_t top_order_p;
    u32 trades_from, trades_width;
    book last_printed_trade;

    mstream bs;
 
    ttime_t dE, dP;
    bool dEdP_printed = false;
    bool paused = false;
    decimal_fixed<price_t> print_p;
    decimal_fixed<count_t> print_c;
    
    volatile bool can_run;
    ::mutex mutex;

    jthread refresh_thrd;

    impl(const mstring& sec, u32 refresh_rate_ms, glass_params gp, trades_params tp) :
        gp(gp), tp(tp),
        sec(sec), refresh_rate(refresh_rate_ms * 1000), ob(),
        auto_scroll(true), top_order_p(), trades_from(gp.view_size(dp)), trades_width(),
        dE(), dP(), can_run(true),
        refresh_thrd(&impl::refresh_thread, this)
    {
    }
    ~impl()
    {
        can_run = false;
    }
    void set_auto_scroll()
    {
        head_msg_auto_scroll = head_msg + (auto_scroll ?
            str_holder("     auto_scroll on ") : str_holder("     auto_scroll off"));
        head_msg_auto_scroll_printed = false;
    }
    void set_instrument(const message_instr& i)
    {
        mi = i;
        head_msg = from_array(mi.exchange_id) + "/" + from_array(mi.feed_id)
            + "/" + from_array(mi.security);
        set_auto_scroll();
    }
    i32 get_top_order(window& w)
    {
        if(auto_scroll || !top_order_p.value)
            return -min<i32>(ob ? ob->bids.size() : 0, (w.rows - 1) / 2);

        if(!ob->bids.empty() && top_order_p.value <= ob->bids.begin()->first.value)
        {
            auto it = ob->bids.lower_bound(top_order_p);
            return -distance(ob->bids.begin(), it);
        }

        auto it = ob->asks.lower_bound(top_order_p);
        return distance(ob->asks.begin(), it);
    }
    void print_trades(window& w)
    {
        if(!trades)
            return;

        if(w.rows <= trades->size())
            trades->pop_front(trades->size() + 1 - w.rows);

        auto& v = *trades->rbegin();
        book tr{v.count, v.time, v.price};

        if(tr == last_printed_trade)
            return;

        u32 i = 0;
        u32 trades_width_limit = w.cols - trades_from;

        for(auto& v : *trades)
        {
            bs << "  ";
            if(tp.print_etime)
                bs << print_td(v.etime, tp.mode) << " ";
            if(tp.print_time)
                bs << print_td(v.time, tp.mode) << " ";

            print_p(bs, v.price, dp.price_decimal);
            if(tp.print_count)
            {
                bs << " ";
                print_c(bs, v.count, dp.count_decimal);
                bs << " " << get_direction(v.direction);
            }

            if(bs.size() < trades_width)
                bs.write(w.blank_row.begin(), trades_width - bs.size());
            else if(bs.size() > trades_width_limit)
                bs.resize(trades_width_limit);
            else
                trades_width = max<u32>(trades_width, bs.size());
            wstr(w, i++, trades_from, bs.begin(), bs.size());
            bs.clear();
        }

        for(; i != w.rows - 1; ++i)
            wstr(w, i, trades_from, w.blank_row.begin(), w.cols - trades_from);

        last_printed_trade = std::move(tr);
    }
    void print_book(bool bids, price_t price, const order_book_leaf& b,
        window& w, u32& row)
    {
        book bo{b.count, b.time, price};
        dp.set(price, b.count);

        if(books_printed[row] != bo)
        {
            e = attron(COLOR_PAIR(bids ? 2 : 3));
            if(gp.print_time)
                bs << print_td(b.time, gp.mode) << " ";

            print_p(bs, price, dp.price_decimal);
            if(gp.print_count)
            {
                bs << " ";
                print_c(bs, b.count, dp.count_decimal);
            }

            if(trades_from > bs.size())
                bs.write(w.blank_row.begin(), trades_from - bs.size());

            wstr(w, row, 0, bs.begin(), bs.size());

            if(bs.size() > trades_from)
            {
                last_printed_trade = book();
                trades_from = bs.size();
            }
            bs.clear();
            books_printed[row] = bo;
        }
        wredrawln(w, row, row);
        ++row;
    }
    void print_order_book(window& w)
    {
        i32 it = get_top_order(w);
        e = attron(A_BOLD);
        u32 row = 0;

        if(it < 0)
        {
            i32 sz = ob->bids.size();
            if(sz)
                --sz;
            auto b = advance(ob->bids.begin(), min(-it, sz));
            auto i = ob->bids.begin();
            for(; row != w.rows - 1; --b)
            {
                ASSERT(b->first != price_t());
                print_book(true, b->first, b->second, w, row);

                if(b == i)
                    break;
            }
        }
        if(ob)
        {
            auto b = ob->asks.begin(), i = ob->asks.end();
            for(; row != w.rows - 1 && b != i; ++b)
            {
                ASSERT(b->first != price_t());
                print_book(false, b->first, b->second, w, row);
            }
        }

        e = attroff(A_BOLD);
        e = attron(COLOR_PAIR(1));

        for(; row != books_printed.size(); ++row)
        {
            if(books_printed[row] == book())
                break;

            books_printed[row] = book();
            wstr(w, row, 0, w.blank_row.begin(), trades_from);
        }
    }
    void print_head(window& w)
    {
        if(!head_msg_auto_scroll_printed)
        {
            wstr(w, w.rows - 1, 0, head_msg_auto_scroll.begin(),
                head_msg_auto_scroll.size());
            head_msg_auto_scroll_printed = true;
        }
        if(!dEdP_printed)
        {
            bs << "dE: " << print_dt(dE.value) << ", dP: " << print_dt(dP.value) << '\0';
            wstr(w, w.rows - 1, w.cols - bs.size(), bs.begin(), bs.size());
            bs.clear();
            dEdP_printed = true;
        }
    }
    void refresh(window& w)
    {
        books_printed.resize(w.rows - 1);
        scoped_lock lock(mutex);

        if(!sec.security_id)
            return;

        print_order_book(w);
        print_trades(w);
        print_head(w);
        lock.unlock();
        move(w.rows - 1, 0);
        wrefresh(w);
    }
    void wait_input(window& w)
    {
        u32 c = refresh_rate / 1000;
        int key = -1;
        for(u32 i = 0; i != c; ++i)
        {
            key = getch();
            if(key == -1)
                usleep(1000);
            else
            {
                if(paused)
                    paused = false;
                else if(key == 112) // 'p'
                    paused = true;

                scoped_lock lock(mutex);
                //mlog() << "key: " << key;

                if(key == KEY_RESIZE)
                {
                    w.clear();
                    w.resize();
                    last_printed_trade = book();
                    head_msg_auto_scroll_printed = false;
                    dEdP_printed = false;
                    print_trades(w);
                }
                else if(paused)
                    continue;
                else if(key == 97) // 'a'
                {
                    auto_scroll = !auto_scroll;
                    set_auto_scroll();
                }
                else if(key == 260 || key == 261) //arrow left || arrow right
                {
                    if(all_securities.size() == 1)
                        break;

                    w.clear();
                    if(!all_securities.empty() && sec.security_id)
                    {
                        auto i = all_securities.find(sec.security_id);
                        if(i != all_securities.end())
                        {
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
                            books_printed.clear();
                        }
                    }
                }
                // arrow up or page up or arrow down or page down
                else if(key == 259 || key == 339 || key == 258 || key == 338)
                {
                    i32 it = get_top_order(w);
                    if(key == 259)
                        --it;
                    else if(key == 339)
                        it -= w.rows - 1;
                    else if(key == 258)
                        ++it;
                    else
                        it += w.rows - 1;
                    if(it <= 0)
                    {
                        if(ob->bids.empty())
                            top_order_p = price_t();
                        else
                        {
                            if(u32(-it) >= ob->bids.size())
                                top_order_p = back(ob->bids).first;
                            else
                                top_order_p = advance(ob->bids.begin(), -it)->first;
                        }
                    }
                    else
                    {
                        if(ob->asks.empty())
                            top_order_p = price_t();
                        else
                        {
                            if(u32(it) >= ob->asks.size())
                                top_order_p = back(ob->asks).first;
                            else
                                top_order_p = advance(ob->asks.begin(), it)->first;
                        }
                    }
                }
                break;
            }
        }
    }
    void refresh_thread()
    {
        try
        {
            window w;
            curs_set(0);
            e = start_color()
                & init_pair(1, COLOR_WHITE, COLOR_BLACK)
                & init_pair(2, COLOR_BLUE, COLOR_YELLOW)
                & init_pair(3, COLOR_BLUE, COLOR_CYAN);

            while(can_run)
            {
                if(!paused)
                    refresh(w);
                wait_input(w);
            }
        }
        catch(exception& e)
        {
            mlog() << "mirror::refresh_thread() " << e;
        }
    }
    void proceed(const message& m)
    {
        if(m.id == msg_ping)
            return;

        u32 security_id = get_security_id(m);
        scoped_lock lock(mutex);
        if(m.id == msg_instr)
            all_securities[security_id] = m.mi;
        if(m.id != msg_trade)
            all_books[security_id].first.proceed(m);
        else
            all_books[security_id].second.push_back(m.mt);

        if(sec.equal(m, security_id))
        {
            if(!ob)
            {
                auto& v = all_books[security_id];
                ob = &v.first;
                trades = &v.second;
            }
            if(m.id == msg_instr)
                set_instrument(m.mi);
            if(m.id == msg_trade)
            {
                ttime_t ct = cur_ttime();
                dE = ct - m.mt.etime;
                dP = ct - m.mt.time;
                dEdP_printed = false;
            }
            else
            {
                if(m.id == msg_book)
                {
                    dP = cur_ttime() - m.mb.time;
                    dEdP_printed = false;
                }
            }
        }
    }
    void proceed(const message* m, u32 count)
    {
        for(u32 i = 0; i != count; ++i, ++m)
            proceed(*m);
    }
};

mirror::impl* create_mirror(str_holder params)
{
    auto p = split(params, ' ');
    if(p.size() > 3)
        throw mexception(es() % "create_mirror, bad params: " % params);

    u32 refresh_rate = p.size() >= 2 ? lexical_cast<u32>(p[1]) : 10;
    if(!refresh_rate)
        throw mexception(es() % "create_mirror, bad params: "
            % params % ", refresh_rate should be at least 1");

    glass_params gp;
    trades_params tp;

    if(p.size() == 3)
    {
        char_cit ie = p[2].end();
        char_cit it = gp.load(p[2].begin(), ie, '|');
        if(it != ie)
            tp.load(it + 1, ie);
    }

    return new mirror::impl(p.empty() ? "$0" : p[0], refresh_rate, gp, tp);
}

mirror::mirror(str_holder params)
{
    pimpl = create_mirror(params);
}

mirror::~mirror()
{
    delete pimpl;
}

void mirror::proceed(const message* m, u32 count)
{
    pimpl->proceed(m, count);
}

void* ying_init(char_cit params)
{
    mlog(mlog::no_cout) << "ying " << _str_holder(params) << " started";
    return create_mirror(_str_holder(params));
}

void ying_destroy(void* w)
{
    delete ((mirror::impl*)(w));
}

void ying_proceed(void* w, const message* m, u32 count)
{
    ((mirror::impl*)(w))->proceed(m, count);
}

