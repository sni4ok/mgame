/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"
#include "types.hpp"

#include "evie/myitoa.hpp"

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
        o.price = m.price;
        o.count.value = o.count.value - m.count.value;
        o.etime = mb.etime;
        o.time = mb.time;

        const price_t& price = mb.price.value ? mb.price : m.price;
        message_book& p = orders_p[price];
        //p.level_id = mb.price.value;
        p.price = price;
        p.count.value = p.count.value + mb.count.value;
        p.etime = mb.etime;
        p.time = mb.time;

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

