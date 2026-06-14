/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "../evie/utils.hpp"
#include "../evie/fmap.hpp"
#include "../evie/profiler.hpp"

//#define USE_PRICE_MAP

#ifdef USE_PRICE_MAP

    #include "../evie/price_map.hpp"
#else

    #include <unordered_map>
    #include <map>

namespace std
{
    template<>
    struct hash<price_t>
    {
        size_t operator()(const price_t& p) const
        {
            return std::hash<u64>()(p.value);
        }
    };
}
#endif

struct order_book_ba
{
    struct book
    {
        count_t count;
        ttime_t time;
    };

    struct message_brief
    {
        price_t price;
        count_t count;
    };

#ifdef USE_PRICE_MAP
    typedef price_map<price_t, message_brief> orders_t;
    typedef price_map<price_t, book> asks_t;
    //typedef fmap<price_t, book, less<price_t> > asks_t;
    typedef fmap<price_t, book, greater<price_t> > bids_t;
#else
    typedef std::unordered_map<price_t, message_brief, std::hash<price_t>, std::equal_to<price_t> > orders_t;
    //typedef std::map<price_t, message_brief> orders_t;
    typedef fmap<price_t, book, less<price_t> > asks_t;
    typedef fmap<price_t, book, greater<price_t> > bids_t;
#endif

    orders_t orders_l;
    asks_t asks;
    bids_t bids;

    void add(price_t price, count_t count, ttime_t time)
    {
        //MPROFILE("order_book_ba::add")
        if(!count)
            return;

        if(count > count_t())
        {
            auto ia = asks.find(price);
            if(ia != asks.end())
            {
                if(ia->second.count <= count)
                {
                    count -= ia->second.count;
                    asks.erase(ia);
                    if(!!count)
                        bids.insert({price, {count, time}});
                }
                else
                {
                    ia->second.count -= count;
                    ia->second.time = time;
                }
            }
            else
            {
                book& b = bids[price];
                b.count += count;
                b.time = time;
            }
        }
        else
        {
            count = -count;
            auto ib = bids.find(price);
            if(ib != bids.end())
            {
                if(ib->second.count <= count)
                {
                    count -= ib->second.count;
                    bids.erase(ib);
                    if(!!count)
                        asks.insert({price, {count, time}});
                }
                else
                {
                    ib->second.count -= count;
                    ib->second.time = time;
                }
            }
            else
            {
                book& a = asks[price];
                a.count += count;
                a.time = time;
            }
        }
    }
    void set(const message_book& mb)
    {
        MPROFILE("order_book::set")
        orders_t::iterator it;
        message_brief* m;
        {
            //MPROFILE("orders_l.[]")

#ifdef USE_PRICE_MAP
            auto v = orders_l.insert(price_t{mb.level_id});
            it = v.first;
#else
            auto v = orders_l.emplace(std::piecewise_construct, std::make_tuple(price_t{mb.level_id}),
                std::make_tuple());
            it = v.first;
#endif

            m = &it->second;
            if(v.second)
                m->price = mb.price;
        }

        const price_t& price = !!mb.price ? mb.price : m->price;
        if(mb.price == m->price || !mb.price)
            add(price, mb.count - m->count, mb.time);
        else
        {
            add(m->price, -m->count, mb.time);
            add(mb.price, mb.count, mb.time);
        }
        if(!mb.count)
        {
            //MPROFILE("orders_l.erase")
            orders_l.erase(it);
        }
        else
        {
            m->price = price;
            m->count = mb.count;
        }
    }
    void proceed(const message& m)
    {
        if(m.id == msg_book)
            set(m.mb);
        else if(m.id == msg_clean || m.id == msg_instr)
        {
            orders_l.clear();
            asks.clear();
            bids.clear();
        }
        else
            throw mexception(es() % "order_book::proceed() unsupported message type: " % m.id.id);
    }
    ~order_book_ba()
    {
    }
};

