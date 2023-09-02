/*
    This file contains Market Data Engine process
    that collect market data from parsers and translate it to consumers (willo, viktor, ying etc)
    Usage: ./makoa_server [config file]

    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "server.hpp"
#include "config.hpp"
#include "signals.hpp"

#include "evie/config.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    if(argc > 2) {
        std::cout << "Usage: ./makoa_server [config file]" << std::endl;
        return 1;
    }
    auto log = log_init(argc == 1 ? "makoa_server.log" : get_log_name(argv[1]).c_str(), mlog::store_tid | mlog::always_cout | mlog::lock_file);
    profilerinfo pff_info;
    init_signals(server_set_close);
    std::string name;
    try {
        mlog(mlog::always_cout) << "makoa started";
        print_init(argc, argv);
        config cfg(argc == 1 ? "makoa_server.conf" : argv[1]);
        cfg.print();
        name = cfg.name;
        engine en(can_run, cfg.pooling, cfg.exports, cfg.export_threads);
        server sv(can_run);
        sv.run(cfg.imports);
    } catch(std::exception& e) {
        mlog(mlog::error | mlog::always_cout) << "makoa(" << name << ") ended with " << e;
        return 1;
    }
    mlog(mlog::always_cout) << "makoa (" << name << ") successfully ended";
    return 0;
}

