/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/fmap.hpp"

static std::string join_tickers(std::vector<std::string> tickers, bool quotes = true)
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
    typedef my_basic_string<char, sizeof(message_instr::security) + 1> ticker;
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
