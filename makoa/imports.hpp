/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <stdint.h>

struct hole_importer
{
    void* (*init)(volatile bool& can_run, const char* params) = 0;
    void (*destroy)(void* c) = 0;
    void (*start)(void* c, void* p) = 0;
    void (*set_close)(void* c) = 0;
};

int register_importer(const char* name, hole_importer hi);
hole_importer create_importer(const char* name);

extern "C"
{
    void* ifile_create(const char* params, volatile bool& can_run);
    void ifile_destroy(void *v);
    uint32_t ifile_read(void *v, char* buf, uint32_t buf_size);
}

