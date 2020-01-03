/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/messages.hpp"

extern "C"
{
    void* viktor_init(const char* params);
    void viktor_destroy(void* v);
    void viktor_proceed(void* v, const message& m);
}

