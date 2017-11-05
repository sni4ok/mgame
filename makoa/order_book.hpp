/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"
#include "types.hpp"

//#include <boost/container/flat_map.hpp>
#include <map>

//TODO: move orders_t to sorted vector

struct message_bytes : msg_head
{
    union
    {
        message_book mb;
        message_trade mt;
        message_clean mc;
        message_instr mi;

        //ping and hello not proceed to export from makoa_server
        message_ping mp;
        message_hello dratuti;
    };
};

struct message : message_bytes
{
    ttime_t mtime; //makoa server time
    bool flush; //when it set, all data from stream is consumed
};

struct order_books
{
    struct node
    {
        count_t count;
        ttime_t time;
        node() : count(), time(){
        }
    };
    //typedef boost::container::flat_map<price_t, node> orders_t;
    typedef std::map<price_t, node> orders_t;

    std::map<uint32_t, orders_t> books;
    std::pair<uint32_t, orders_t*> last_value;

    orders_t& get_orders(uint32_t security_id)
    {
        if(security_id == last_value.first)
            return *last_value.second;
        orders_t& o = books[security_id];
        last_value = {security_id, &o};
        return o;
    }
    orders_t& add(const message_book& mb)
    {
        orders_t& o = get_orders(mb.security_id);
        node& n = o[mb.price];
        n.count.value += mb.count.value;
        n.time = mb.time;
        return o;
    }
    orders_t& set(const message_book& mb)
    {
        orders_t& o = get_orders(mb.security_id);
        node& n = o[mb.price];
        n.count.value = mb.count.value;
        n.time = mb.time;
        return o;
    }
    orders_t& clear(uint32_t security_id)
    {
        orders_t& o = get_orders(security_id);
        o.clear();
        return o;
    }
    orders_t& proceed(const message& m)
    {
        if(m.id == msg_book)
            return set(m.mb);
        else if(m.id == msg_clean)
            return clear(m.mc.security_id);
        else if(m.id == msg_instr)
            return clear(m.mi.security_id);
        else
            throw std::runtime_error(es() % "order_book::proceed() unsupported message type: " % m.id);
    }
    void compact() //remove levels with zero count
    {
        for(auto& o: books)
        {
            auto i = o.second.begin();
            while(i != o.second.end())
            {
                if(!i->second.count.value)
                    i = o.second.erase(i);
                else
                    ++i;
            }
        }
    }
};

template<typename stream>
stream& operator<<(stream& s, const order_books::orders_t& o)
{
    s << "order_book [" << o.size() << "]";
    for(auto&& v: o)
        s << "\n  " << v.second.time << " " << v.first << " " << v.second.count;
    return s;
}

