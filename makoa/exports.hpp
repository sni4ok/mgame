/*
    this module contains lifetime holder for dynamyc (*.so) data consumers,
    makoa shares logger with them
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include <memory>

struct exports
{
    struct impl;
    void proceed(const message& m);
    exports(const std::string& module);
    exports(const std::string& module, const std::string& params);
    ~exports();

private:
    std::unique_ptr<impl> pimpl;
};

struct hole_exporter
{
    void* (*init)(const char* params);
    void (*destroy)(void*);
    void (*proceed)(void*, const message& m);
};

