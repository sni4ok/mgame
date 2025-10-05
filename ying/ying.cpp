/*
   Ying
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "../evie/mlog.hpp"

#include "../makoa/exports.hpp"

void* ying_init(char_cit params);
void ying_destroy(void* w);
void ying_proceed(void* w, const message* m, u32 count);

extern "C"
{
    void create_hole(hole_exporter* m, exporter_params params)
    {
        init_exporter_params(params);
        m->init = &ying_init;
        m->destroy = &ying_destroy;
        m->proceed = &ying_proceed;
    }
}

