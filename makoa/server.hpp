/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <memory>

struct server
              // used modules: logger, config, engine, securities
{
    struct impl;
    server();
    ~server();
    void accept_loop(volatile bool& can_run);

private:
    std::unique_ptr<impl> pimpl;
};
