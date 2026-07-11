/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/mstring.hpp"

struct engine
{
    class impl;
    impl* pimpl;

    engine(volatile bool& can_run, bool pooling, const mvector<mstring>& exports, u32 export_threads,
        bool set_engine_time = false);
    engine(const engine&) = delete;
    ~engine();
};

