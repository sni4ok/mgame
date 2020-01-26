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

struct read_time_impl
{

    my_basic_string<char, 11> cur_date;
    uint64_t cur_date_time;
    template<uint32_t frac_size>
    ttime_t read_time(const char* &it)
    {
        //2020-01-26T10:45:21 //frac_size 0
        //2020-01-26T10:45:21.418 //frac_size 3
        //2020-01-26T10:45:21.418000001 //frac_size 9

        if(unlikely(cur_date != str_holder(it, 10)))
        {
            if(*(it + 4) != '-' || *(it + 7) != '-')
                throw std::runtime_error(es() % "bad time: " % std::string(it, it + 26));
            struct tm t = tm();
            int y = my_cvt::atoi<int>(it, 4); 
            int m = my_cvt::atoi<int>(it + 5, 2); 
            int d = my_cvt::atoi<int>(it + 8, 2); 
            t.tm_year = y - 1900;
            t.tm_mon = m - 1;
            t.tm_mday = d;
            cur_date_time = timegm(&t) * my_cvt::p10<9>();
            cur_date = str_holder(it, 10);
            mlog() << "cur_date set " << cur_date;
        }
        it += 10;
        if(*it != 'T' || *(it + 3) != ':' || *(it + 6) != ':' || (frac_size ? *(it + 9) != '.' : false))
            throw std::runtime_error(es() % "bad time: " % std::string(it - 10, it + 10 + (frac_size ? 1 + frac_size : 0)));
        uint64_t h = my_cvt::atoi<uint64_t>(it + 1, 2);
        uint64_t m = my_cvt::atoi<uint64_t>(it + 4, 2);
        uint64_t s = my_cvt::atoi<uint64_t>(it + 7, 2);
        uint64_t ns = 0;
        if(frac_size)
        {
            uint64_t frac = my_cvt::atoi<uint64_t>(it + 10, frac_size);
            ns = frac * my_cvt::p10<9 - frac_size>();
            it += (frac_size + 1);
        }
        it += 9;
        return ttime_t{cur_date_time + ns + (s + m * 60 + h * 3600) * my_cvt::p10<9>()};
    }
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

