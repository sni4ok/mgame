/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/fmap.hpp"

mstring join_tickers(const mvector<mstring>& tickers, bool quotes = true);

template<typename base>
struct sec_id_by_name : base
{
    typedef my_basic_string<sizeof(message_instr::security) + 1> ticker;
    uint32_t get_security_id(char_cit i, char_cit ie, ttime_t time)
    {
        ticker symbol(i, ie);
        auto it = securities.find(symbol);
        if(unlikely(it == securities.end())) {
            tmp.init(config::instance().exchange_id, config::instance().feed_id, mstring(i, ie));
            securities[symbol] = tmp.mi.security_id;
            tmp.proceed_instr(this->e, time);
            return tmp.mi.security_id;
        }
        else
            return it->second;
    }

private:
    fmap<ticker, uint32_t> securities;
    security tmp;
};

template<typename func>
auto read_value(char_cit &it, char_cit ie, func f, bool last)
{
    skip_fixed(it, "\"");
    char_cit ne = find(it, ie, '\"');
    auto ret = f(it, ne);
    it = ne + 1;
    if(!last)
        skip_fixed(it, ",");
    return ret;
}

inline str_holder read_str(char_cit it, char_cit ie)
{
    return str_holder(it, ie - it);
}

template<typename func, typename array>
auto read_named_value(const array& v, char_cit& it, char_cit ie, char last, func f)
{
    static_assert(std::is_array<array>::value);
    skip_fixed(it, v);
    char_cit ne = find(it, ie, last);
    auto ret = f(it, ne);
    it = ne + 1;
    return ret;
}

