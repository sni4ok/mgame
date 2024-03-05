/*
    this module contains lifetime holder for dynamyc (*.so) data consumers,
    makoa shares logger with them
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "../evie/mstring.hpp"

struct hole_exporter
{
    void* (*init)(const char* params) = 0;
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

    ~exporter() {
        if(p && he.destroy)
            he.destroy(p);
    }
    void proceed(const message* m, uint32_t count) {
        he.proceed(p, m, count);
    }
};

uint32_t register_exporter(str_holder module, hole_exporter he);

inline ttime_t get_export_mtime(const message* m)
{
    return (m - 1)->t.time;
}

void set_export_mtime(message* m);

class simple_log;
class profilerinfo;

struct exporter_params
{
    simple_log* sl;
    profilerinfo* pf;
    volatile bool* can_run;
};

void set_can_run(volatile bool* can_run);
void init_exporter_params(exporter_params params);

