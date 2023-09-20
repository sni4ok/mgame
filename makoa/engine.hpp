/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/vector.hpp"
#include "evie/mstring.hpp"

struct engine // wrapper for control lifetime of consumers
              // used modules: logger
{
    class impl;
    engine(volatile bool& can_run, bool pooling, const mvector<mstring>& exports, uint32_t export_threads);
    engine(const engine&) = delete;
    ~engine();

private:
    impl* pimpl;
};

