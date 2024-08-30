/*
    config for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/mstring.hpp"
#include "../evie/singleton.hpp"

struct config : stack_singleton<config>
{
    mstring name;

    mvector<mstring> imports;
    mvector<mstring> exports;
    uint32_t export_threads;

    bool pooling;
    config(char_cit fname);
    void print();
};

