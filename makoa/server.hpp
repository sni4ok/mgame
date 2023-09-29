/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/mstring.hpp"

struct server
{
    struct impl;
    server(volatile bool& can_run, bool quit_on_exit = false);
    ~server();
    void run(const mvector<mstring>& imports);
    server(const server&) = delete;

private:
    impl* pimpl;
};

void server_set_close();

