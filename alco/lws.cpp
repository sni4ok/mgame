/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/string.hpp"
#include "evie/mstring.hpp"

mstring join_tickers(const mvector<mstring>& tickers, bool quotes)
{
    my_stream s;
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

