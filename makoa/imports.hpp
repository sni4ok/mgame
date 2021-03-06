/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/string.hpp"

struct hole_importer
{
    void* (*init)(volatile bool& can_run, const char* params) = 0;
    void (*destroy)(void* c) = 0;
    void (*start)(void* c, void* p) = 0;
    void (*set_close)(void* c) = 0;
};

uint32_t register_importer(const std::string& name, hole_importer hi);
hole_importer create_importer(const char* name);

