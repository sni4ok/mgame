/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <memory>
#include <vector>

struct server
{
    struct impl;
    server(volatile bool& can_run, bool quit_on_exit = false);
    ~server();
    void run(const std::vector<std::string>& imports);

private:
    std::unique_ptr<impl> pimpl;
};

void server_set_close();

