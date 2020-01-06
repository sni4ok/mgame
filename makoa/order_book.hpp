/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"
#include "types.hpp"

#include "evie/fmap.hpp"
#include "evie/myitoa.hpp"

struct order_book : fmap<price_t, message_book>
{
    uint32_t orders;

    order_book() : orders()
    {
    }

    void set(const message_book& mb)
    {
        message_book& m = (*this)[mb.price];
        if(!m.security_id) {
            m.security_id = mb.security_id;
            m.price = mb.price;
            ++orders;
        } else {
            if(unlikely(m.security_id != mb.security_id))
                throw std::runtime_error(es() % "order_book cross securities detected, old: " % m.security_id % ", new: " % mb.security_id);

            if(m.count.value && !mb.count.value)
                --orders;
            else if(!m.count.value && mb.count.value)
                ++orders;
        }
        m.count = mb.count;
        m.etime = mb.etime;
        m.time = mb.time;
    }
    void proceed(const message& m)
    {
		if(m.id == msg_book)
            return set(m.mb);
        else if(m.id == msg_clean || m.id == msg_instr) {
            clear();
            orders = 0;
        }
        else
            throw std::runtime_error(es() % "order_book::proceed() unsupported message type: " % m.id.id);

    }
};

