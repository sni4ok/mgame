/*
    this module contains lifetime holder for dynamyc (*.so) data consumers,
    makoa shares logger with them
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include "evie/utils.hpp"

#include <list>
#include <map>

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
    exporter(const std::string& params);

    exporter(exporter&& r);
    void operator=(exporter&& r);
    ~exporter() {
        if(he.destroy)
            he.destroy(p);
    }
    void proceed(const message* m, uint32_t count) {
        he.proceed(p, m, count);
    }
};

uint32_t register_exporter(const std::string& module, hole_exporter he);

