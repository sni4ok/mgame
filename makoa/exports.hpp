/*
    this module contains lifetime holder for dynamyc (*.so) data consumers,
    makoa shares logger with them
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "../evie/mstring.hpp"
#include "../evie/smart_ptr.hpp"

struct hole_exporter
{
    void* (*init)(char_cit params) = 0;
    void (*destroy)(void*) = 0;
    void (*proceed)(void* v, const message* m, uint32_t count) = 0;
};

struct exporter
{
    void *p;
    hole_exporter he;

    exporter() : p(), he()
    {
    }
    exporter(const mstring& params);
    exporter(exporter&& r);
    void operator=(exporter&& r);
    ~exporter();

    void proceed(const message* m, uint32_t count) {
        he.proceed(p, m, count);
    }
};

inline ttime_t get_export_mtime(const message* m)
{
    return (m - 1)->t.time;
}

inline void set_export_mtime(message* m)
{
    (m - 1)->t.time = cur_ttime();
}

class simple_log;
struct exports_factory;
void free_exports_factory(exports_factory* ptr);
unique_ptr<exports_factory, free_exports_factory> init_efactory();
void register_exporter(exports_factory *ef, str_holder module, hole_exporter he);

struct exporter_params
{
    simple_log* sl;
    volatile bool* can_run;
    exports_factory* efactory;
};

void set_can_run(volatile bool* can_run);
void init_exporter_params(exporter_params params);

