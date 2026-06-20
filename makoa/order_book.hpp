/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "../evie/utils.hpp"
#include "../evie/fmap.hpp"
#include "../evie/price_map.hpp"
#include "../evie/mlog.hpp"

#include <unordered_map>

//#define USE_PRICE_MAP

struct message_brief
{
    price_t price;
    count_t count;
};

struct order_book_leaf
{
    count_t count;
    ttime_t time;
};

template<typename orders_t, typename asks_t, typename bids_t>
class order_book_base
{
    orders_t orders;

public:

    asks_t asks;
    bids_t bids;

    void add(price_t price, count_t count, ttime_t time)
    {
        //MPROFILE("order_book::add")
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
                order_book_leaf& b = bids[price];
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
                order_book_leaf& a = asks[price];
                a.count += count;
                a.time = time;
            }
        }
    }
    void set(const message_book& mb)
    {
        MPROFILE("order_book::set")
        MPROFILE_COUNT("order_book::level_id", {mb.level_id})

        message_brief* m = orders.get(mb.level_id, mb.price);

        const price_t& price = !!mb.price ? mb.price : m->price;
        if(mb.price == m->price || !mb.price)
            add(price, mb.count - m->count, mb.time);
        else
        {
            add(m->price, -m->count, mb.time);
            add(mb.price, mb.count, mb.time);
        }
        if(!mb.count)
            orders.erase_prev();
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
            orders.clear();
            if(m.id == msg_instr)
                orders.set(m.mi);
            asks.clear();
            bids.clear();
        }
        else
            throw mexception(es() % "order_book::proceed() unsupported message type: " % m.id.id);
    }
    ~order_book_base()
    {
    }
};

struct order_book_orders_t
{
    virtual void set(const message_instr& mi) = 0;
    virtual message_brief* get(i64 level_id, price_t price) = 0;
    virtual void erase_prev() = 0;
    virtual void clear() = 0;

    virtual ~order_book_orders_t() = default;
};

struct unordered_orders_t : order_book_orders_t
{
    typedef std::unordered_map<i64, message_brief> orders_t;
    orders_t orders;
    orders_t::iterator it;

    void set(const message_instr&)
    {
    }
    message_brief* get(i64 level_id, price_t price)
    {
        auto v = orders.emplace(std::piecewise_construct, std::make_tuple(level_id),
            std::make_tuple());

        it = v.first;

        if(v.second)
            it->second.price = price;

        return &it->second;
    }
    void erase_prev()
    {
        orders.erase(it);
    }
    void clear()
    {
        orders.clear();
    }
};

struct price_map_orders_t : order_book_orders_t
{
    typedef price_map<price_t, message_brief> orders_t;
    orders_t orders;
    orders_t::iterator it;

    void set(const message_instr&)
    {
    }
    message_brief* get(i64 level_id, price_t price)
    {
        auto v = orders.insert(price_t{level_id});
        it = v.first;

        if(v.second)
            it->second.price = price;

        return &it->second;
    }
    void erase_prev()
    {
        orders.erase(it);
    }
    void clear()
    {
        orders.clear();
    }
};

struct bid_ask_orders_t : order_book_orders_t
{
    pair<bool, message_brief> values[3];
    i64 prev;

    void set(const message_instr&)
    {
    }
    message_brief* get(i64 level_id, price_t price)
    {
        assert(level_id == 1 || level_id == 2);
        auto& [b, m] = values[level_id];
        if(!b)
        {
            b = true;
            m.price = price;
            m.count = count_t();
        }

        prev = level_id;
        return &m;
    }
    void __erase(i64 level_id)
    {
        ::get<0>(values[level_id]) = false;
    }
    void erase_prev()
    {
        __erase(prev);
    }
    void clear()
    {
        __erase(1);
        __erase(2);
    }
};

struct dynamic_orders_t
{
    order_book_orders_t* orders = nullptr;

    void set(const message_instr& mi)
    {
        if(!orders)
        {
            str_holder exchange = from_array(mi.exchange_id);
            if(from_any(exchange, "crypcom", "huobi", "kraken"))
                orders = new price_map_orders_t;
            if(from_any(exchange, "binance", "bybit"))
                orders = new bid_ask_orders_t;
            else
            {
                mlog() << "dynamic_orders_t exchange not found: " << exchange
                    << ", ticker: " << from_array(mi.security);
                orders = new unordered_orders_t;
            }
        }
    }
    message_brief* get(i64 level_id, price_t price)
    {
        return orders->get(level_id, price);
    }
    void erase_prev()
    {
        orders->erase_prev();
    }
    void clear()
    {
        if(orders)
            orders->clear();
    }
    ~dynamic_orders_t()
    {
        delete orders;
    }
};

struct order_book_ba : order_book_base<
#ifdef USE_PRICE_MAP
    dynamic_orders_t,
    //price_map_orders_t,
    price_map<price_t, order_book_leaf>,
    price_map<price_t, order_book_leaf, false>
#else
    unordered_orders_t,
    fmap<price_t, order_book_leaf, less<price_t> >,
    fmap<price_t, order_book_leaf, greater<price_t> >
#endif
    >
{
};

