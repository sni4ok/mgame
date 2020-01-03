/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"
#include "config.hpp"

#include <memory>

struct engine // wrapper for control lifetime of consumers
              // used modules: logger, config
{
    class impl;
    engine();
    ~engine();

private:
    std::unique_ptr<impl> pimpl;
};

//proceed input data by engine
//in case of exception connection should be closed
struct context;
struct context_holder : noncopyable
{
    context* ctx;
    context_holder();
    ~context_holder();
};

uint32_t proceed_data(const uint8_t* data, uint32_t size, context* ctx);

