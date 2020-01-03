/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "viktor.hpp"
#include "makoa/engine.hpp"
#include "makoa/exports.hpp"
#include "makoa/types.hpp"

#include "tyra/tyra.hpp"

namespace {

struct viktor
{
    std::string host;
    std::unique_ptr<tyra> ty;

    viktor(const std::string& params)
    {
        auto ib = params.begin(), ie = params.end();
        auto i = std::find(ib, ie, ' ');
        if(i == ie || i + 1 == ie)
            throw std::runtime_error(es() % "viktor::viktor() bad params: " % params);

        std::string mode(ib, i);
        if(mode != "sight")
            throw std::runtime_error("viktor::viktor() write me");

        host = std::string(i + 1, ie);
        connect();
    }

    void connect()
    {
        if(!ty)
            ty.reset(new tyra(host));
    }

    void proceed(const message& m)
    {
        if(m.id == msg_book) {
            auto& v = reinterpret_cast<const tyra_msg<message_book>& >(m);
            ty->send(v);
        }
        else if(m.id == msg_trade) {
            auto& v = reinterpret_cast<const tyra_msg<message_trade>& >(m);
            ty->send(v);
        }
        else if(m.id == msg_clean) {
            auto& v = reinterpret_cast<const tyra_msg<message_clean>& >(m);
            ty->send(v);
        }
        else if(m.id == msg_instr) {
            auto& v = reinterpret_cast<const tyra_msg<message_instr>& >(m);
            ty->send(v);
        }
        else if(m.id == msg_ping) {
            tyra_msg<message_ping>& v = (tyra_msg<message_ping>&)m;
            ty->send(v);
        }
        if(m.flush) {
            ty->flush();
        }
    }
};

}

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

    void viktor_proceed(void* v, const message& m)
    {
        ((viktor*)(v))->proceed(m);
    }

    void create_hole(hole_exporter* m, simple_log* sl)
    {
        log_set(sl);
        m->init = &viktor_init;
        m->destroy = &viktor_destroy;
        m->proceed = &viktor_proceed;
    }
}

