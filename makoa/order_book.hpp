/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "order_b.hpp"

#include "../evie/utils.hpp"
#include "../evie/fmap.hpp"
#include "../evie/price_map.hpp"
#include "../evie/mlog.hpp"

#include <unordered_map>

#define USE_PRICE_MAP

template<typename orders_t, typename asks_t, typename bids_t>
class order_book_base
{
    orders_t orders;

public:
    asks_t asks;
    bids_t bids;

    void proceed(const message& m)
    {
        if(m.id == msg_book)
            proceed_message_book(m.mb, orders, asks, bids);
        else if(m.id == msg_clean || m.id == msg_instr)
        {
            orders.clear();
            asks.clear();
            bids.clear();
            if(m.id == msg_instr)
                orders.set(m.mi);
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
    virtual u64 max_elements() = 0;

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
    u64 max_elements()
    {
        return limits<u64>::max;
    }
};

template<typename orders_t = price_map<price_t, message_brief> >
struct price_map_orders_t : order_book_orders_t
{
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
    u64 max_elements()
    {
        return orders.elems;
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
    u64 max_elements()
    {
        return 2;
    }
};

struct dynamic_orders_t
{
    order_book_orders_t* orders = nullptr;
    bool check_price_overflow = false;
    price_t max_price;

    void set(const message_instr& mi)
    {
        if(!orders)
        {
            str_holder exchange = from_array(mi.exchange_id);

            if(from_any(exchange, "bitfinex", "bitmex"))
            {
                //orders = new price_map_orders_t<price_map<price_t, message_brief, true
                //   , 0, true, ib3, ib3> >;
                orders = new unordered_orders_t;
            }
            else if(from_any(exchange, "binance", "bybit"))
                orders = new bid_ask_orders_t;
            else if(from_any(exchange, "crypcom"))
            {
                orders = new price_map_orders_t;
                //check_price_overflow = true;
                //max_price = price_t{1000 * i64(orders->max_elements())};
            }
            else if(from_any(exchange, "huobi", "kraken"))
                orders = new price_map_orders_t;
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
        if(check_price_overflow)
        {
            if(price > max_price || price.value % 1000)
            {
                MPROFILE("dynamic_orders_t, price skipped")
                //mlog() << "dynamic_orders_t, price skipped " << price;
                return nullptr;
            }
        }
        return orders->get(level_id, price);
    }
    void erase_prev()
    {
        MPROFILE("dynamic_orders_t, erase_prev")
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

