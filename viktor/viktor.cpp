/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "viktor.hpp"
#include "makoa/exports.hpp"
#include "makoa/types.hpp"

#include "evie/vector.hpp"
#include "evie/fmap.hpp"

#include <deque>
#include <map>

struct viktor
{
    mvector<message_instr> instrs;
    pmap<uint32_t, std::deque<message_book> > messages;
    message_times last_times;

    std::string params;
    std::unique_ptr<exporter> e;
    time_t last_connect;

    typedef std::map<int64_t/*level_id*/, message_book> snapshot;
    std::map<uint32_t, snapshot> snapshots;
    std::vector<message> s;

    viktor(const std::string& params) : last_times(), params(params), last_connect()
    {
    }
    void clear(uint32_t security_id)
    {
        messages[security_id].clear();
        snapshots[security_id].clear();
    }
    void save(const message* m, uint32_t count)
    {
        for(uint32_t i = 0; i != count; ++i, ++m)
        {
            if(m->id == msg_book)
            {
                messages[m->mb.security_id].push_back(m->mb);
            }
            else if(m->id == msg_instr)
            {
                auto it = std::find_if(instrs.begin(), instrs.end(),
                    [m](const message_instr& ins)
                    {
                        return ins.security_id == m->mi.security_id;
                    }
                );
                if(it != instrs.end())
                    (message_times&)(*it) = m->t;
                else
                {
                    instrs.push_back(m->mi);
                    messages[m->mi.security_id];
                }
                clear(m->mi.security_id);
            }
            else if(m->id == msg_clean)
            {
                clear(m->mc.security_id);
            }
        }
    }
    void send_snapshots()
    {
        s.clear();

        for(message_instr& mi: instrs)
        {
            static_cast<message_times&>(mi) = last_times;
            s.push_back((message&)mi);
        }

        for(const auto& mes: messages)
        {
            snapshot& sn = snapshots[mes.first];
            for(const message_book& m: mes.second)
            {
                message_book& mb = sn[m.level_id];
                price_t price = m.price.value ? m.price : mb.price;
                assert(price.value);
                mb = m;
                mb.price = price;
            }
        }

        for(auto& mes: messages)
            mes.second.clear();

        for(auto& sn: snapshots)
        for(auto& mb: sn.second)
        {
            if(mb.second.count != count_t())
            {
                static_cast<message_times&>(mb.second) = last_times;
                s.push_back((message&)mb.second);
            }
        }

        if(!s.empty())
            e->proceed(&s[0], s.size());
    }
    void reconnect()
    {
        time_t ct = time(NULL);
        if(ct > last_connect + 5)
        {
            last_connect = ct;
            e.reset(new exporter(params));
            send_snapshots();
        }
    }
    void proceed(const message* m, uint32_t count)
    {
        try
        {
            if(!unlikely(e))
                reconnect();
            last_times = (m + count - 1)->mt;
            if(likely(e))
                e->proceed(m, count);
        }
        catch(std::exception& exc)
        {
            mlog() << "viktor::proceed() " << exc;
            e.reset();
        }
        save(m, count);
    }
};

extern "C"
{
    void* viktor_init(const char* params)
    {
        return new viktor(params);
    }

    void viktor_destroy(void* v)
    {
        delete (viktor*)(v);
    }

    void viktor_proceed(void* v, const message* m, uint32_t count)
    {
        ((viktor*)(v))->proceed(m, count);
    }

    void create_hole(hole_exporter* m, exporter_params params)
    {
        init_exporter_params(params);
        m->init = &viktor_init;
        m->destroy = &viktor_destroy;
        m->proceed = &viktor_proceed;
    }
}

