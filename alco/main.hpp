/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/signals.hpp"

int parser_main(int argc, char** argv, const std::string& parser_name, void (*proceed)(volatile bool&))
{
    if(argc > 2) {
        std::cout << "Usage: ./" << parser_name << " [config file]" << std::endl;
        return 1;
    }
    auto log = log_init(argc == 1 ? (parser_name + ".log").c_str() : get_log_name(argv[1]).c_str(), mlog::always_cout | mlog::info);
    profilerinfo pff_info;
    init_signals();
    try {
        mlog() << parser_name << " started";
        print_init(argc, argv);
        config cfg(argc == 1 ? (parser_name + ".conf").c_str() : argv[1]);
        proceed(can_run);

    } catch(std::exception& e) {
        mlog(mlog::error) << parser_name << " ended with " << e;
        return 1;
    }
    mlog() << parser_name << " successfully ended";
    return 0;
}

