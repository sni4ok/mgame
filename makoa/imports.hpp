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

typedef uint32_t u32;

extern "C"
{
    void* ifile_create(const char* params, volatile bool& can_run);
    void ifile_destroy(void *v);
    u32 ifile_read(void *v, char* buf, u32 buf_size);

    void* files_replay_create(const char* params, volatile bool& can_run);
    void files_replay_destroy(void *v);
    u32 files_replay_read(void *v, char* buf, u32 buf_size);
}

