/*
    config for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/utils.hpp"

struct config : stack_singleton<config>
{
    std::string name;

    std::vector<std::string> imports;
    std::vector<std::string> exports;
    uint32_t export_threads;

    bool pooling;
    config(const char* fname);
    void print();
};

