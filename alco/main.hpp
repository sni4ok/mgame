/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/utils.hpp"
#include "../makoa/exports.hpp"

#include "../evie/signals.hpp"
#include "../evie/config.hpp"
#include "../evie/mlog.hpp"

int parser_main(int argc, char** argv, str_holder parser, void (*proceed)(volatile bool&),
    void (*term_signal_func)(int) = nullptr)
{
    mstring parser_name(parser);
    unique_ptr<simple_log, free_simple_log> log;

    if(argc > 2)
    {
        cout() << "Usage: ./" << parser_name << " [config file]";
        return 1;
    }
    try
    {
        log = log_init(argc == 1 ? (parser_name + ".log").c_str() :
            get_log_name(_str_holder(argv[1])).c_str(),
            mlog::always_cout | mlog::info | mlog::lock_file);

        signals_holder sl(term_signal_func);

        mlog() << parser_name << " started";
        auto ef = init_efactory();
        print_init(argc, argv);
        config cfg(argc == 1 ? (parser_name + ".conf").c_str() : argv[1]);
        proceed(can_run);

    }
    catch(exception& e)
    {
        conditional_stream<mlog, cerr>(!log, mlog::error | mlog::always_cout, true)
            << parser_name << " ended with " << e;
        return 1;
    }
    mlog() << parser_name << " successfully ended";
    return 0;
}

