/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/mstring.hpp"

struct engine // wrapper for control lifetime of consumers
              // used modules: logger
{
    class impl;
    engine(volatile bool& can_run, bool pooling, const mvector<mstring>& exports, uint32_t export_threads,
        bool set_engine_time = false);
    engine(const engine&) = delete;
    ~engine();

private:
    impl* pimpl;
};

