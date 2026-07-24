/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "order_b.hpp"

#include "../evie/fmap.hpp"
#include "../evie/price_map.hpp"
#include "../evie/mlog.hpp"

#include <unordered_map>

//#define USE_PRICE_MAP

template<typename orders_t, typename asks_t, typename bids_t>
struct order_book
{
    orders_t orders;
    asks_t asks;
    bids_t bids;
    bool need_init = true, relative = false, bba;

    void proceed(const message& m)
    {
        MPROFILE("order_book, proceed")
        if(m.id == msg_book)
        {
            if(need_init)
            {
                bba = from_any(m.mb.level_id, 1, 2);
                need_init = false;
            }

            if(relative)
                proceed_message_book(m.mb, orders, asks, bids);
            else if(bba)
                proceed_message_bba(m.mb, asks, bids);
            else
                set_message_book_abs(m.mb.price, m.mb.count, m.mb.time, asks, bids);
        }
        else if(m.id == msg_clean || m.id == msg_instr)
        {
            orders.clear();
            asks.clear();
            bids.clear();
            if(m.id == msg_instr)
            {
                str_holder exchange = from_array(m.mi.exchange_id);
                relative = from_any(exchange, "bitfinex", "bitmex");
            }
        }
        else
            throw mexception(es() % "order_book, proceed unsupported message type: " % m.id.id);
    }
    ~order_book()
    {
    }
};

struct unordered_orders_t
{
    typedef std::unordered_map<i64, message_brief> orders_t;
    orders_t orders;
    orders_t::iterator it;

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

struct order_book_ba : order_book<
#ifdef USE_PRICE_MAP
    unordered_orders_t,
    price_map<price_t, book_leaf>,
    price_map<price_t, book_leaf, false>
#else
    unordered_orders_t,
    fmap<price_t, book_leaf, less<price_t> >,
    fmap<price_t, book_leaf, greater<price_t> >
#endif
    >
{
};

