/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <memory>

struct server
{
    struct impl;
    server(volatile bool& can_run);
    ~server();
    void import_loop();

private:
    std::unique_ptr<impl> pimpl;
};
