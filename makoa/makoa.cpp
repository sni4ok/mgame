/*
    This file contains Market Data Engine process
    that collect market data from parsers and translate it to consumers (willo, viktor, ying etc)
    Usage: ./makoa_server [config file]

    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "server.hpp"
#include "securities.hpp"

#include "evie/log.hpp"

#include <iostream>

#include <signal.h>

volatile bool can_run = true;

void term_signal(int sign)
{
    mlog() << "Termination signal received: " << sign;
    can_run = false;
}

void on_signal(int sign)
{
    mlog() << "Signal received: " << sign;
}

int main(int argc, char** argv)
{
    signal(SIGTERM, &term_signal);
    signal(SIGINT, &term_signal);
    signal(SIGHUP, &on_signal);
    signal(SIGPIPE, SIG_IGN);
    if(argc > 2) {
        std::cout << "Usage: ./makoa_server [config file]" << std::endl;
        return 1;
    }
    log_init li(argc == 1 ? "makoa_server.log" : get_log_name(argv[1]), mlog::cout | mlog::info);
    std::string name;
    try {
        mlog(mlog::cout) << "makoa started";
        print_init(argc, argv);
        config cfg(argc == 1 ? "makoa_server.conf" : argv[1]);
        cfg.print();
        name = cfg.name;
        securities sec;
        engine en;
        server sv;
        sv.accept_loop(can_run);
    } catch(std::exception& e) {
        mlog(mlog::error | mlog::cout) << "makoa(" << name << ") ended with " << e;
        return 1;
    }
    mlog(mlog::cout) << "makoa (" << name << ") successfully ended";
    return 0;
}

