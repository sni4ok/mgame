/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "evie/utils.hpp"
#include "evie/fmap.hpp"

#include <map>

struct order_book
{
    void set(const message_book& mb)
    {
        message_book& m = orders_l[mb.level_id];
        if(!m.security_id) {
            m.security_id = mb.security_id;
            m.price = mb.price;
        } else if(unlikely(m.security_id != mb.security_id))
            throw std::runtime_error(es() % "order_book cross securities detected, old: " % m.security_id % ", new: " % mb.security_id);

        assert(m.price.value);
        message_book& o = orders_p[m.price];
        //o.level_id = m.price.value;
        o.count.value = o.count.value - m.count.value;
        if(!o.count.value)
            orders_p.erase(m.price);
        else {
            o.price = m.price;
            o.etime = mb.etime;
            o.time = mb.time;
        }

        const price_t& price = mb.price.value ? mb.price : m.price;
        message_book& p = orders_p[price];
        //p.level_id = mb.price.value;
        p.count.value = p.count.value + mb.count.value;
        if(!p.count.value)
            orders_p.erase(price);
        else {
            p.price = price;
            p.etime = mb.etime;
            p.time = mb.time;
        }

        m.level_id = mb.level_id;
        m.price = price;
        m.count = mb.count;
        m.etime = mb.etime;
        m.time = mb.time;
    }
    void proceed(const message& m)
    {
		if(m.id == msg_book)
            return set(m.mb);
        else if(m.id == msg_clean || m.id == msg_instr) {
            orders_l.clear();
            orders_p.clear();
        }
        else
            throw std::runtime_error(es() % "order_book::proceed() unsupported message type: " % m.id.id);
    }

    std::map<int64_t, message_book> orders_l;
    std::map<price_t, message_book> orders_p;
    typedef std::map<price_t, message_book>::const_iterator price_iterator;
};

template<typename allocator = std::allocator<std::pair<const int64_t, message_book> > >
struct order_book_ba
{
    struct book
    {
        count_t count;
        ttime_t time;
    };

    void add(price_t price, count_t count, ttime_t time)
    {
        //MPROFILE("order_book_ba::add")
        if(!count.value)
            return;

        if(count.value > 0)
        {
            auto ia = asks.find(price);
            if(ia != asks.end())
            {
                if(ia->second.count.value <= count.value)
                {
                    count.value -= ia->second.count.value;
                    asks.erase(ia);
                    if(count.value)
                        bids.insert({price, {count, time}});
                }
                else
                {
                    ia->second.count.value -= count.value;
                    ia->second.time = time;
                }
            }
            else
            {
                book& b = bids[price];
                b.count.value += count.value;
                b.time = time;
            }
        }
        else
        {
            count.value = -count.value;
            auto ib = bids.find(price);
            if(ib != bids.end())
            {
                if(ib->second.count.value <= count.value)
                {
                    count.value -= ib->second.count.value;
                    bids.erase(ib);
                    if(count.value)
                        asks.insert({price, {count, time}});
                }
                else
                {
                    ib->second.count.value -= count.value;
                    ib->second.time = time;
                }
            }
            else
            {
                book& a = asks[price];
                a.count.value += count.value;
                a.time = time;
            }
        }
    }
    void set(const message_book& mb)
    {
        //MPROFILE("order_book::set")
        message_book& m = orders_l[mb.level_id];
        if(!m.security_id)
        {
            m.security_id = mb.security_id;
            m.price = mb.price;
        }
        else if(unlikely(m.security_id != mb.security_id))
            throw std::runtime_error(es() % "order_book_ba cross securities detected, old: " % m.security_id % ", new: " % mb.security_id);
        const price_t& price = mb.price.value ? mb.price : m.price;

        if(mb.price.value == m.price.value || !mb.price.value)
        {
            add(price, count_t({mb.count.value - m.count.value}), mb.time);
        }
        else
        {
            add(m.price, count_t({-m.count.value}), mb.time);
            add(mb.price, count_t({mb.count.value}), mb.time);
        }
        if(!mb.count.value)
            orders_l.erase(mb.level_id);
        else
        {
            m.level_id = mb.level_id;
            m.price = price;
            m.count = mb.count;
            m.etime = mb.etime;
            m.time = mb.time;
        }
    }
    void proceed(const message& m)
    {
        if(m.id == msg_book)
            return set(m.mb);
        else if(m.id == msg_clean || m.id == msg_instr) {
            orders_l.clear();
            asks.clear();
            bids.clear();
        }
        else
            throw std::runtime_error(es() % "order_book::proceed() unsupported message type: " % m.id.id);
    }
    ~order_book_ba()
    {
        //mlog() << "~order_book_ba: ol_sz: " << orders_l.size() << ", asz: " << asks.size() << ", bsz: " << bids.size();
    }

    std::map<int64_t, message_book, std::less<int64_t>, allocator> orders_l;
    fmap<price_t, book, std::less<price_t> > asks;
    fmap<price_t, book, std::greater<price_t> > bids;
};

