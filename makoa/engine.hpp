/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <memory>
#include <vector>

struct engine // wrapper for control lifetime of consumers
              // used modules: logger
{
    class impl;
    engine(volatile bool& can_run, bool pooling, const std::vector<std::string>& exports, uint32_t export_threads);
    ~engine();

private:
    std::unique_ptr<impl> pimpl;
};

