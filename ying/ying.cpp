/*
   Ying
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mirror.hpp"
#include "makoa/exports.hpp"

struct ying : stack_singleton<ying>
{
    mirror m;

    ying(const std::string& params) : m(params)
    {
        mlog() << "ying " << params << " started";
    }

    void proceed(const message& mes)
    {
        m.proceed(mes);
    }
};

void* ying_init(std::string params)
{
    return new ying(params);
}

void ying_destroy(void* w)
{
    delete (ying*)(w);
}

void ying_proceed(const message& m)
{
    ying::instance().proceed(m);
}

extern "C"
{
    void create_hole(hole_exporter* m, simple_log* sl)
    {
        log_set(sl);
        m->init = &ying_init;
        m->destroy = &ying_destroy;
        m->proceed = &ying_proceed;
    }
}

