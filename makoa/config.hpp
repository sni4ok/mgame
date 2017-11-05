/*
    config for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/config.hpp"
#include "evie/log.hpp"

struct config : stack_singleton<config>
{
    bool log_exporter;
    uint16_t port;
    std::string name;

    //export can be set by two ways,
    //multiple modules in one row without configuration params:
    //  exports = module(,module2)
    //or with params, one row for each module:
    //  export = module[ params]

    std::vector<std::string> exports;

    config(const char* fname);
    void print();
};

