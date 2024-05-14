/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../makoa/types.hpp"

#include "../evie/fmap.hpp"
#include "../evie/algorithm.hpp"

mstring join_tickers(const mvector<mstring>& tickers, bool quotes = true);

template<typename base>
struct sec_id_by_name : base
{
    typedef my_basic_string<sizeof(message_instr::security) + 1> ticker;
    uint32_t get_security_id(char_cit i, char_cit ie, ttime_t time)
    {
        ticker symbol(i, ie);
        auto it = securities.find(symbol);
        if(it == securities.end()) [[unlikely]] {
            tmp.init(config::instance().exchange_id, config::instance().feed_id, mstring(i, ie));
            securities[symbol] = tmp.mi.security_id;
            tmp.proceed_instr(this->e, time);
            return tmp.mi.security_id;
        }
        else
            return it->second;
    }

    using base::base;

private:
    fmap<ticker, uint32_t> securities;
    security tmp;
};

extern "C"
{
    extern size_t strnlen(const char*, size_t) __THROW __attribute_pure__ __nonnull ((1));
}

template<typename str>
inline void skip_fixed(char_cit& it, const str& v)
{
    static_assert(is_array_v<str>);
    //bool eq = std::equal(it, it + sizeof(v) - 1, v);
    //bool eq = !(strncmp(it, v, sizeof(v) - 1));
    bool eq = !(memcmp(it, v, sizeof(v) - 1));
    if(!eq) [[unlikely]]
        throw mexception(es() % "skip_fixed error, expect: |" % str_holder(v) % "| in |" % str_holder(it, strnlen(it, 100)) % "|");
    it += sizeof(v) - 1;
}

template<typename str>
inline void search_and_skip_fixed(char_cit& it, char_cit ie, const str& v)
{
    static_assert(is_array_v<str>);
    char_cit i = search(it, ie, v, v + (sizeof(v) - 1));
    if(i == ie) [[unlikely]]
        throw mexception(es() % "search_and_skip_fixed error, expect: |" % str_holder(v) % "| in |" % str_holder(it, ie - it) % "|");
    it  = i + (sizeof(v) - 1);
}

template<typename str>
inline bool skip_if_fixed(char_cit& it, const str& v)
{
    static_assert(is_array_v<str>);
    //bool eq = std::equal(it, it + sizeof(v) - 1, v);
    //bool eq = !(strncmp(it, v, sizeof(v) - 1));
    bool eq = !(memcmp(it, v, sizeof(v) - 1));
    if(eq)
        it += sizeof(v) - 1;
    return eq;
}
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
    static_assert(is_array_v<array>);
    skip_fixed(it, v);
    char_cit ne = find(it, ie, last);
    auto ret = f(it, ne);
    it = ne + 1;
    return ret;
}

