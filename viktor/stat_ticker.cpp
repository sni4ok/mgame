/*
    author: Ilya Andronov <sni4ok@yandex.ru>

    export = stat_ticker
*/

#include "makoa/order_book.hpp"
#include "makoa/exports.hpp"
#include "makoa/types.hpp"

#include "evie/math.hpp"

#include <map>
#include <set>
#include <optional>

namespace
{
    struct stat
    {
        struct info
        {
            std::optional<price_t> min_trade, max_trade, min_ask, max_ask, min_bid, max_bid;
            uint64_t asks, bids;
            std::set<price_t> trades_p, asks_p, bids_p;
            std::multiset<count_t> trades;

            order_book_ba<> ob;
            ttime_t first_ob_time, last_ob_time;
            std::multiset<price_t> spreads;

            info() : asks(), bids(), first_ob_time(), last_ob_time()
            {
            }
        };
        std::map<uint32_t, std::pair<message_instr, info> > data;

        void proceed(const message* m)
        {
            if(m->id == msg_book) {
                const price_t& p = m->mb.price;
                info& v = (data.at(m->mb.security_id)).second;
                if(m->mb.count.value < 0)
                {
                    v.min_ask = v.min_ask ? std::min(*v.min_ask, p) : p;
                    v.max_ask = v.max_ask ? std::max(*v.max_ask, p) : p;
                    ++v.asks;
                    v.asks_p.insert(p);
                }
                else if(m->mb.count.value > 0)
                {
                    v.min_bid = v.min_bid ? std::min(*v.min_bid, p) : p;
                    v.max_bid = v.max_bid ? std::max(*v.max_bid, p) : p;
                    ++v.bids;
                    v.bids_p.insert(p);
                }
                v.ob.proceed(*m);
                if(!v.first_ob_time.value)
                    v.first_ob_time = m->mb.time;
                if(v.last_ob_time != m->mb.time && !v.ob.asks.empty() && !v.ob.bids.empty())
                {
                    v.spreads.insert({v.ob.asks.begin()->first.value - v.ob.bids.begin()->first.value});
                    v.last_ob_time = m->mb.time;
                }
            }
            else if(m->id == msg_trade) {
                const price_t& p = m->mt.price;
                info& v = (data.at(m->mt.security_id)).second;
                v.min_trade = v.min_trade ? std::min(*v.min_trade, p) : p;
                v.max_trade = v.max_trade ? std::max(*v.max_trade, p) : p;
                v.trades_p.insert(p);
                if(m->mt.count.value)
                    v.trades.insert(m->mt.count);
            }
            else if(m->id == msg_instr) {
                std::pair<message_instr, info>& v = data[m->mi.security_id];
                v.first = m->mi;
                v.second.ob.proceed(*m);
            }
        }
        void proceed(const message* m, uint32_t count)
        {
            for(uint32_t i = 0; i != count; ++i, ++m)
              proceed(m);
        }
        ~stat()
        {
            mlog ml;
            for(auto&& v: data)
            {
                const info& i = v.second.second;
                ml << "exchange: " << v.second.first.exchange_id << ", feed: " << v.second.first.feed_id
                    << ", security: " << v.second.first.security << ", security_id: " << v.second.first.security_id << "\n"
                    << "  from " << i.first_ob_time << " to " << i.last_ob_time << "\n";
                if(i.min_trade)
                    ml << "  min_trade: " << *i.min_trade;
                if(i.max_trade)
                    ml << " max_trade: " << *i.max_trade;
                if(i.min_bid)
                    ml << " min_bid: " << *i.min_bid;
                if(i.max_bid)
                    ml << " max_bid: " << *i.max_bid;
                if(i.min_ask)
                    ml << " min_ask: " << *i.min_ask;
                if(i.max_ask)
                    ml << " max_ask: " << *i.max_ask;
                auto get_pips = [](const std::set<price_t>& prices)
                {
                    if(prices.empty())
                        return price_t();
                    auto it = prices.begin(), ie = prices.end();
                    price_t price = *it;
                    ++it;
                    price_t min_price = price_t(abs(price.value - it->value));
                    for(; it != ie; ++it) {
                        min_price = std::min(min_price, price_t(abs(price.value - it->value)));
                        price = *it;
                    }
                    return min_price;
                };
                uint64_t trades = i.trades.size();
                ml << "\n  trades: " << trades << "(min price step " << get_pips(i.trades_p)  << ") bids: "
                    << i.bids << "(min price step " << get_pips(i.bids_p) << ") asks: " << i.asks << "(min price step " << get_pips(i.asks_p) << ")";
                if(!i.trades.empty()) {
                    auto it = i.trades.begin();
                    ml << "\n  min_trade_count: " << *it;
                    std::advance(it, trades / 10);
                    ml << " 10%_trades_count: " << *it;
                    std::advance(it, 4 * trades / 10);
                    ml << " 50%_trades_count: " << *it;
                    std::advance(it, 4 * trades / 10);
                    ml << " 90%_trades_count: " << *it
                       << " max_trade_count: " << *i.trades.rbegin();
                }
                uint64_t spreads = i.spreads.size();
                ml << "\n  spreads: " << spreads;
                if(!i.spreads.empty()) {
                    auto it = i.spreads.begin();
                    ml << " min_spread: " << *it;
                    std::advance(it, spreads / 100);
                    ml << " 1%_spreads: " << *it;
                    it = i.spreads.begin();
                    std::advance(it, spreads / 10);
                    ml << " 10%_spreads: " << *it;
                    std::advance(it, 4 * spreads / 10);
                    ml << " 50%_spreads: " << *it;
                    std::advance(it, 4 * spreads / 10);
                    ml << " 90%_spreads: " << *it;
                    auto ie = i.spreads.rbegin();
                    std::advance(ie, spreads / 100);
                    ml << " 99%_spreads: " << *ie
                       << " max_spread: " << *i.spreads.rbegin();
                }
                ml << "\n";
            }
        }
    };

    void* stat_init(const char*)
    {
        return new stat();
    }
    void stat_destroy(void* v)
    {
        delete (stat*)(v);
    }
    void stat_proceed(void* v, const message* m, uint32_t count)
    {
        ((stat*)(v))->proceed(m, count);
    }
}

extern "C"
{
    void create_hole(hole_exporter* m, exporter_params params)
    {
        init_exporter_params(params);
        m->init = &stat_init;
        m->destroy = &stat_destroy;
        m->proceed = &stat_proceed;
    }
}
