/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mirror.hpp"
#include "ncurses.hpp"

#include "../makoa/types.hpp"
#include "../makoa/order_book.hpp"

#include "../evie/thread.hpp"
#include "../evie/mstring.hpp"
#include "../evie/queue.hpp"

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

bool is_dec_uint(str_holder s)
{
    for(char c: s)
    {
        if(c < '0' || c > '9')
            return false;
    }
    return true;
}

static const str_holder first_ticker = "$0";

struct security_filter
{
    mstring security;
    u32 security_id = 0;

    void init(const mstring& s = first_ticker)
    {
        security = s;
        if(is_dec_uint(s.str()))
            security_id = lexical_cast<u32>(s);
    }
    bool equal(const message& m, u32 sec_id)
    {
        if(security_id == sec_id)
            return true;
        if(m.id == msg_instr)
        {
            if(!security_id && from_any(security, from_array(m.mi.security), first_ticker))
                security_id = m.mi.security_id;
            return m.mi.security_id == security_id;
        }
        return false;
    }
};

pair<int, char> trade_color(u32 direction)
{
    if(direction == 0)
        return {1, 'U'};
    if(direction == 1)
        return {2, 'B'};
    if(direction == 2)
        return {3, 'S'};
    throw mexception(es() % "bad direction: " % direction);
}

struct print_dt
{
    i64 value;
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

void wstr(window& w, int y, int x, char_cit str, int n)
{
    MPROFILE("mirror, wstr")
    if(!n)
        return;

    ASSERT(y >= 0 && x >= 0 && n > 0 && u32(y) < w.rows && u32(x) < w.cols);
    if(x + n + 1 > int(w.cols))
        n = w.cols - x - 1;
    e = mvwaddnstr(w, y, x, str, n);
}

void clear_rectangle(window& w, int yf, int yn, int xf, int xn)
{
    MPROFILE("mirror, clear_rectangle")
    for(int y = yf; y != yf + yn; ++y)
        e = mvwaddnstr(w, y, xf, w.blank_row.begin(), xn);
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
    u32 price_decimal = 0,
        count_decimal = 0;

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
    int mode = print_td::ms;
    bool print_time = true,
         print_count = true;

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
            throw_exception("mirror_params error: ", str_holder(f, t));
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

struct book_view
{
    security_filter sec;
    order_book_ba* ob = nullptr;
    queue<message_trade>* trades;
    mvector<book> books_printed;
    message_instr mi;
    mstring head_msg;
    bool head_msg_printed = false;
    price_t top_order_p = price_t();
    book last_printed_trade = book();

    book_view(const mstring& security = first_ticker) : sec(security), mi()
    {
    }
    void clear_view()
    {
        top_order_p = price_t();
        fill(books_printed.begin(), books_printed.end(), book());
        last_printed_trade = book();
        head_msg_printed = false;
    }
    void set_instrument(const message_instr& i)
    {
        mi = i;
        head_msg = from_array(i.exchange_id) + "/" + from_array(i.feed_id)
            + "/" + from_array(i.security);
        head_msg_printed = false;
    }
    void set(const book_view& c)
    {
        sec = c.sec;
        ob = c.ob;
        trades = c.trades;
        mi = c.mi;
        head_msg = c.head_msg;
    }
};

struct mirror::impl
{
    fmap<u32, message_instr> all_securities;
    std::unordered_map<u32, pair<order_book_ba, queue<message_trade> > > all_books;

    decimal_params dp;
    glass_params gp;
    trades_params tp;

    u32 refresh_rate;
    bool auto_scroll = true,
         paused = false;
    volatile bool can_run = true;
    u32 trades_from, trades_width = 0;
    bool have_updates = false;

    mstream bs;
    ttime_t dE = ttime_t(), dP = ttime_t();
    bool dEdP_printed = false;
    decimal_fixed<price_t> print_p;
    decimal_fixed<count_t> print_c;
    ::mutex mutex;
    jthread refresh_thrd;

    u32 x_views, y_views;
    mvector<book_view> views;
    u32 x = 0, y = 0;
    u32 __rows = 0, __cols = 0;

    impl(u32 refresh_rate_ms, glass_params gp, trades_params tp,
            u32 x_views, u32 y_views, const mvector<str_holder>& securities) :
        gp(gp), tp(tp), refresh_rate(refresh_rate_ms * 1000),
        trades_from(gp.view_size(dp)),
        x_views(x_views), y_views(y_views)
    {
        views.resize(x_views * y_views);
        for(u32 i = 0; i != views.size(); ++i)
        {
            if(securities.size() > i)
                views[i].sec.init(securities[i]);
            else
                views[i].sec.init();
        }
        refresh_thrd = jthread(&impl::refresh_thread, this);
    }
    ~impl()
    {
        can_run = false;
    }
    book_view& cur_book()
    {
        return views[x + y * x_views];
    }
    void clear_books_printed()
    {
        for(auto& v: views)
            fill(v.books_printed.begin(), v.books_printed.end(), book());
    }
    void set_axis(window& w)
    {
        __rows = (w.rows - 1) / y_views;
        __cols = w.cols / x_views;
        ASSERT(__rows && __cols);
        for(auto& v: views)
            v.books_printed.resize(__rows - 1);
        clear_books_printed();
    }
    void print_auto_scroll(window& w)
    {
        str_holder s = auto_scroll ? str_holder("auto_scroll on ")
            : str_holder("auto_scroll off");
        wstr(w, w.rows - 1, 0, s.begin(), s.size());
    }
    void clear_trades()
    {
        for(u32 i = 0; i != x_views * y_views; ++i)
            views[i].last_printed_trade = book();
    }
    i32 get_top_order(book_view& c)
    {
        if(auto_scroll || !c.top_order_p)
            return -min<i32>(c.ob ? c.ob->bids.size() : 0, (__rows - 1) / 2);

        if(!c.ob->bids.empty() && c.top_order_p <= c.ob->bids.begin()->first)
        {
            auto it = c.ob->bids.lower_bound(c.top_order_p);
            return -distance(c.ob->bids.begin(), it);
        }

        auto it = c.ob->asks.lower_bound(c.top_order_p);
        return distance(c.ob->asks.begin(), it);
    }
    u32 rows_from() const
    {
        return y * __rows;
    }
    u32 cols_from() const
    {
        return x * __cols;
    }
    void print_trades(window& w, bool& r)
    {
        MPROFILE("mirror, print_trades")
        book_view& c = cur_book();
        if(!c.trades)
            return;

        if(__rows - 1 < c.trades->size())
            c.trades->pop_front(c.trades->size() - __rows + 1);

        auto& v = *c.trades->rbegin();
        book tr{v.price, v.count, v.time};

        if(tr == c.last_printed_trade || __cols <= trades_from + 1)
            return;

        r = true;

        u32 i = rows_from() + 1, ie = i + __rows - 1;
        u32 trades_width_limit = __cols - trades_from - 1;
        u32 cx = cols_from() + trades_from;

        for(const auto& v : *c.trades)
        {
            auto p = trade_color(v.direction);
            e = attron(COLOR_PAIR(p.first));
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
                bs << " " << p.second;
            }

            bool print = true;
            if(bs.size() < trades_width)
            {
                wstr(w, i, cx, bs.begin(), bs.size());
                e = attron(COLOR_PAIR(1));
                wstr(w, i++, cx + bs.size(), w.blank_row.begin(), trades_width - bs.size());
                print = false;
            }
            else if(bs.size() > trades_width_limit)
                bs.resize(trades_width_limit);
            else
                trades_width = max<u32>(trades_width, bs.size());

            if(print)
            {
                wstr(w, i++, cx, bs.begin(), bs.size());
                e = attron(COLOR_PAIR(1));
            }

            bs.clear();
        }

        for(; i != ie; ++i)
            wstr(w, i, cx, w.blank_row.begin(), __cols - trades_from);

        c.last_printed_trade = std::move(tr);
    }
    void print_book(book_view& c, window& w, bool bids, price_t price,
        const book_leaf& b, u32& row, u32 row_f, bool& r)
    {
        MPROFILE("mirror, print_book")
        book bo{price, b.count, b.time};
        dp.set(price, b.count);

        if(c.books_printed[row - row_f] != bo)
        {
            r = true;
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

            if(bs.size() >= __cols)
                bs.resize(__cols - 1);

            wstr(w, row, cols_from(), bs.begin(), bs.size());

            if(bs.size() > trades_from)
            {
                clear_trades();
                trades_from = bs.size();
            }
            bs.clear();
            c.books_printed[row - row_f] = bo;
            wredrawln(w, row, 1);
        }
        ++row;
    }
    void print_order_book(window& w, bool& r)
    {
        MPROFILE("mirror, print_order_book")
        book_view& c = cur_book();
        i32 it = get_top_order(c);
        e = attron(A_BOLD);
        u32 row = rows_from() + 1, re = row + __rows - 1, row_f = row;
        ASSERT(re <= w.rows);

        if(it < 0)
        {
            i32 sz = c.ob->bids.size();
            if(sz)
                --sz;
            auto b = advance(c.ob->bids.begin(), min(-it - 1, sz));
            auto i = c.ob->bids.begin();
            for(; row != re; --b)
            {
                ASSERT(b->first != price_t());
                print_book(c, w, true, b->first, b->second, row, row_f, r);
                if(b == i)
                    break;
            }
        }
        if(c.ob)
        {
            auto b = c.ob->asks.begin(), i = c.ob->asks.end();
            for(; row != re && b != i; ++b)
            {
                ASSERT(b->first != price_t());
                print_book(c, w, false, b->first, b->second, row, row_f, r);
            }
        }

        e = attroff(A_BOLD);
        e = attron(COLOR_PAIR(1));

        u32 cr = row - row_f;
        u32 cf = cols_from();
        for(; cr != c.books_printed.size(); ++cr, ++row)
        {
            r = true;
            if(c.books_printed[cr] == book())
                break;

            c.books_printed[cr] = book();
            wstr(w, row, cf, w.blank_row.begin(), trades_from);
        }
    }
    void print_head(window& w, bool& r)
    {
        MPROFILE("mirror, print_head")
        if(!dEdP_printed)
        {
            r = true;
            bs << "dE: " << print_dt(dE.value) << ", dP: " << print_dt(dP.value);
            if(w.cols > bs.size())
                wstr(w, w.rows - 1, w.cols - bs.size(), bs.begin(), bs.size());
            bs.clear();
            dEdP_printed = true;
        }
    }
    void refresh(window& w)
    {
        MPROFILE("mirror, refresh")
        scoped_lock lock(mutex);
        if(!have_updates)
            return;

        bool r = false;
        have_updates = false;
        u32 cx = x, cy = y;
        for(u32 i = 0; i != x_views; ++i)
        for(u32 ii = 0; ii != y_views; ++ii)
        {
            x = i;
            y = ii;

            book_view& c = cur_book();
            if(!c.head_msg_printed && !c.head_msg.empty())
            {
                r = true;
                if(x == cx && y == cy)
                    e = attron(COLOR_PAIR(4));
                wstr(w, rows_from(), cols_from(), c.head_msg.begin(), c.head_msg.size());
                if(x == cx && y == cy)
                    e = attron(COLOR_PAIR(1));
                c.head_msg_printed = true;
            }
            print_order_book(w, r);
            print_trades(w, r);
            print_head(w, r);
        }

        x = cx;
        y = cy;

        lock.unlock();
        if(r)
        {
            move(w.rows - 1, 0);
            wrefresh(w);
        }
    }
    void proceed_key(window& w, int key)
    {
        MPROFILE("mirror, proceed_key")
        if(paused)
            paused = false;
        else if(key == 112) // 'p'
        {
            paused = true;
            return;
        }

        scoped_lock lock(mutex);
        //mlog() << "key: " << key;

        if(key == KEY_RESIZE)
        {
            w.resize();
            w.clear();
            clear_trades();
            clear_books_printed();
            set_axis(w);
            print_auto_scroll(w);
            dEdP_printed = false;
        }
        else if(key == 97) // 'a'
        {
            auto_scroll = !auto_scroll;
            print_auto_scroll(w);
        }
        else if(from_any(key, 9, 353)) // tab, shift + tab
        {
            if(x_views == 1 && y_views == 1)
                return;

            cur_book().head_msg_printed = false;

            if(key == 9)
            {
                ++x;
                if(x == x_views)
                {
                    x = 0;
                    ++y;
                    if(y == y_views)
                        y = 0;
                }
            }
            else
            {
                if(x)
                    --x;
                else
                {
                    x = x_views - 1;
                    if(y)
                        --y;
                    else
                        y = y_views - 1;
                }
            }

            cur_book().head_msg_printed = false;
        }
        else if(from_any(key, 56, 50, 54, 52)) //8, 2, 6, 4
        {
            static const u32 max_x = 7, max_y = 7;

            if(key == 56)
            {
                if(y_views >= max_y)
                    return;

                u32 f = views.size();
                ++y_views;
                views.resize(views.size() + x_views);
                for(; f != views.size(); ++f)
                {
                    views[f].sec.init();
                    views[f].set(views[0]);
                }
            }
            else if(key == 50)
            {
                if(y_views == 1)
                    return;

                views.resize(u32(views.size() - x_views));
                --y_views;

                if(y == y_views)
                {
                    --y;
                    cur_book().head_msg_printed = false;
                }
            }
            else if(key == 54)
            {
                if(x_views >= max_x)
                    return;

                for(u32 i = y_views; i != 0; --i)
                {
                    auto it = views.insert(views.begin() + i * x_views, book_view());
                    it->set(views[0]);
                }
                ++x_views;
            }
            else if(key == 52)
            {
                if(x_views == 1)
                    return;

                auto it = views.end() - 1;
                for(u32 i = 0;; ++i)
                {
                    it = views.erase(it);
                    if(i == y_views - 1)
                        return;
                    it -= x_views - 1;
                }
                --x_views;

                if(x == x_views)
                {
                    --x;
                    cur_book().head_msg_printed = false;
                }
            }

            for(book_view& c: views)
                c.clear_view();
            set_axis(w);
            w.clear();
        }
        else if(from_any(key, 260, 261)) //arrow left, arrow right
        {
            if(all_securities.size() == 1)
                return;

            book_view& c = cur_book();
            if(!all_securities.empty() && c.sec.security_id)
            {
                auto i = all_securities.find(c.sec.security_id);
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
                    c.ob = &v.first;
                    c.trades = &v.second;
                    c.sec.security_id = i->first;
                    c.set_instrument(i->second);
                    c.clear_view();
                    clear_rectangle(w, rows_from(), __rows, cols_from(), __cols);
                }
            }
        }
        // arrow up, page up, arrow down, page down
        else if(from_any(key, 259, 339, 258, 338))
        {
            book_view& c = cur_book();
            i32 it = get_top_order(c);
            if(key == 259)
                --it;
            else if(key == 339)
                it -= __rows;
            else if(key == 258)
                ++it;
            else
                it += __rows;
            if(it <= 0)
            {
                if(!c.ob || c.ob->bids.empty())
                    c.top_order_p = price_t();
                else
                {
                    if(u32(-it) >= c.ob->bids.size())
                        c.top_order_p = back(c.ob->bids).first;
                    else
                        c.top_order_p = advance(c.ob->bids.begin(), -it)->first;
                }
            }
            else
            {
                if(!c.ob || c.ob->asks.empty())
                    c.top_order_p = price_t();
                else
                {
                    if(u32(it) >= c.ob->asks.size())
                        c.top_order_p = back(c.ob->asks).first;
                    else
                        c.top_order_p = advance(c.ob->asks.begin(), it)->first;
                }
            }
        }
    }
    void wait_input(window& w)
    {
        int key = -1;
        {
            MPROFILE("mirror, getch")
            key = getch();
        }
        if(key == -1)
            usleep(refresh_rate);
        else
        {
            proceed_key(w, key);
            have_updates = true;
        }
    }
    void refresh_thread()
    {
        try
        {
            window w;
            set_axis(w);
            print_auto_scroll(w);
            curs_set(0);
            e = start_color()
                & init_pair(1, COLOR_WHITE, COLOR_BLACK)
                & init_pair(2, COLOR_BLUE, COLOR_CYAN)
                & init_pair(3, COLOR_BLUE, COLOR_YELLOW)
                & init_pair(4, COLOR_CYAN, COLOR_BLACK);

            while(can_run)
            {
                wait_input(w);

                if(!paused)
                    refresh(w);
            }
        }
        catch(exception& e)
        {
            mlog() << "mirror::refresh_thread() " << e;
        }
    }
    void proceed(const message& m)
    {
        MPROFILE("mirror, proceed");
        if(m.id == msg_ping)
            return;
        u32 security_id = get_security_id(m);
        if(m.id == msg_instr)
            all_securities[security_id] = m.mi;
        if(m.id != msg_trade)
            all_books[security_id].first.proceed(m);
        else
            all_books[security_id].second.push_back(m.mt);

        for(book_view& c: views)
        {
            if(c.sec.equal(m, security_id))
            {
                if(!c.ob)
                {
                    auto& v = all_books[security_id];
                    c.ob = &v.first;
                    c.trades = &v.second;
                }
                if(m.id == msg_instr)
                    c.set_instrument(m.mi);
                else if(m.id == msg_trade)
                {
                    ttime_t ct = cur_ttime();
                    dE = ct - m.mt.etime;
                    dP = ct - m.mt.time;
                    dEdP_printed = false;
                }
                else if(m.id == msg_book)
                {
                    dP = cur_ttime() - m.mb.time;
                    dEdP_printed = false;
                }
            }
        }
    }
    void proceed(const message* m, u32 count)
    {
        scoped_lock lock(mutex);
        for(u32 i = 0; i != count; ++i, ++m)
            proceed(*m);
        have_updates = true;
    }
};

mirror::impl* create_mirror(str_holder params)
{
    mvector<str_holder> p = split(params, ' ');

    u32 x = 1, y = 1;
    if(!p.empty() && str_holder(p[0].begin(), p[0].begin() + 7) == "matrix:")
    {
        char_cit it = p[0].begin() + 7, ie = p[0].end(),
        ne = find(it, ie, 'x');
        if(ne == ie)
            throw str_exception("ying, matrix should be like matrix:2x2");
        x = lexical_cast<u32>(it, ne);
        ++ne;
        y = lexical_cast<u32>(ne, ie);
        if(!x || !y)
            throw str_exception("ying, matrix should be positive");
        p.erase(p.begin());
    }

    if(p.size() > 3)
        throw mexception(es() % "ying, bad params: " % params);

    u32 refresh_rate = p.size() >= 2 ? lexical_cast<u32>(p[1]) : 10;
    if(!refresh_rate)
        throw mexception(es() % "ying, bad params: "
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

    mvector<str_holder> securities;
    if(!p.empty())
        securities = split(p[0]);

    return new mirror::impl(refresh_rate, gp, tp, x, y, securities);
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

