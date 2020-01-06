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

        std::string host = std::string(i + 1, ie);
        ty.reset(new tyra(host));
    }

    void proceed(const message* m, uint32_t count)
    {
        ty->send(m, count);
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

    void viktor_proceed(void* v, const message* m, uint32_t count)
    {
        ((viktor*)(v))->proceed(m, count);
    }

    void create_hole(hole_exporter* m, simple_log* sl)
    {
        log_set(sl);
        m->init = &viktor_init;
        m->destroy = &viktor_destroy;
        m->proceed = &viktor_proceed;
    }
}

