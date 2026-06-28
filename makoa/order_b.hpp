/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "../evie/profiler.hpp"

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

void add_message_book(price_t price, count_t count, ttime_t time, auto& asks, auto& bids)
{
    //MPROFILE("add_message_book")
    if(!count)
        return;

    auto f = [price, &count, time](auto& f, auto& t)
    {
        auto ia = f.find(price);
        if(ia != f.end())
        {
            if(ia->second.count <= count)
            {
                count -= ia->second.count;
                f.erase(ia);
                if(!!count)
                    t.insert({price, {count, time}});
            }
            else
            {
                ia->second.count -= count;
                ia->second.time = time;
            }
        }
        else
        {
            order_book_leaf& b = t[price];
            b.count += count;
            b.time = time;
        }
    };

    if(count > count_t())
        f(asks, bids);
    else
    {
        count = -count;
        f(bids, asks);
    }
}

void proceed_message_book(const message_book& mb, auto& orders, auto& asks, auto& bids)
{
    MPROFILE("order_book::set")
    MPROFILE_COUNT("order_book::level_id", {mb.level_id})

    message_brief* m = orders.get(mb.level_id, mb.price);
    if(!m)
        return;

    const price_t& price = !!mb.price ? mb.price : m->price;

    if(mb.price == m->price || !mb.price)
        add_message_book(price, mb.count - m->count, mb.time, asks, bids);
    else
    {
        add_message_book(m->price, -m->count, mb.time, asks, bids);
        add_message_book(mb.price, mb.count, mb.time, asks, bids);
    }

    if(!mb.count)
        orders.erase_prev();
    else
    {
        m->price = price;
        m->count = mb.count;
    }
}

