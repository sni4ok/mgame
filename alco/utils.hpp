/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/fmap.hpp"

#include <sstream>

inline std::string join_tickers(std::vector<std::string> tickers, bool quotes = true)
{
    std::stringstream s;
    for(uint32_t i = 0; i != tickers.size(); ++i) {
        if(i)
            s << ",";
        if(quotes)
            s << "\"";
        s << tickers[i];
        if(quotes)
            s << "\"";
    }
    return s.str();
}

template<typename base>
struct sec_id_by_name : base
{
    typedef my_basic_string<sizeof(message_instr::security) + 1> ticker;
    uint32_t get_security_id(const char* i, const char* ie, ttime_t time)
    {
        ticker symbol(i, ie);
        auto it = securities.find(symbol);
        if(unlikely(it == securities.end())) {
            tmp.init(config::instance().exchange_id, config::instance().feed_id, std::string(i, ie));
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
auto read_value(const char* &it, const char* ie, func f, bool last)
{
    skip_fixed(it, "\"");
    const char* ne = std::find(it, ie, '\"');
    auto ret = f(it, ne);
    it = ne + 1;
    if(!last)
        skip_fixed(it, ",");
    return ret;
}

inline str_holder read_str(const char* it, const char* ie)
{
    return str_holder(it, ie - it);
}

template<typename func, typename array>
auto read_named_value(const array& v, const char* &it, const char* ie, char last, func f)
{
    static_assert(std::is_array<array>::value);
    skip_fixed(it, v);
    const char* ne = std::find(it, ie, last);
    auto ret = f(it, ne);
    it = ne + 1;
    return ret;
}

