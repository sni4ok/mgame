/*
    This file contains Market Data Engine process
    that collect market data from parsers and translate it to consumers (willo, viktor, ying etc)
    Usage: ./makoa_server [config file]

    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "server.hpp"

#include "evie/mlog.hpp"
#include "evie/profiler.hpp"

#include <iostream>
#include <csignal>

volatile bool can_run = true;

void server_set_close();
void term_signal(int sign)
{
    mlog() << "Termination signal received: " << sign;
    can_run = false;
    server_set_close();
}

void on_signal(int sign)
{
    mlog() << "Signal received: " << sign;
}

int main(int argc, char** argv)
{
    std::signal(SIGTERM, &term_signal);
    std::signal(SIGINT, &term_signal);
    std::signal(SIGHUP, &on_signal);
    std::signal(SIGPIPE, &on_signal);
    if(argc > 2) {
        std::cout << "Usage: ./makoa_server [config file]" << std::endl;
        return 1;
    }
    log_raii li(argc == 1 ? "makoa_server.log" : get_log_name(argv[1]), mlog::store_tid | mlog::always_cout | mlog::lock_file);
    profilerinfo pff_info;
    std::string name;
    try {
        mlog(mlog::always_cout) << "makoa started";
        print_init(argc, argv);
        config cfg(argc == 1 ? "makoa_server.conf" : argv[1]);
        cfg.print();
        name = cfg.name;
        engine en(can_run);
        server sv(can_run);
        sv.import_loop();
    } catch(std::exception& e) {
        mlog(mlog::error | mlog::always_cout) << "makoa(" << name << ") ended with " << e;
        return 1;
    }
    mlog(mlog::always_cout) << "makoa (" << name << ") successfully ended";
    return 0;
}

