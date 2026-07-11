/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef parser_name
    #define PP_STR(text) PP_STR_I(text)
    #define PP_STR_I(...) #__VA_ARGS__
    #define parser_name str_holder(PP_STR(ns))
#endif

#include "../makoa/exports.hpp"

#include "../evie/signals.hpp"
#include "../evie/config.hpp"
#include "../evie/mlog.hpp"
#include "../evie/cond_stream.hpp"
#include "../evie/string.hpp"

namespace ns
{
    void proceed_parser(volatile bool& can_run);
}

extern void (*term_signal_func)(int);

int main(int argc, char** argv)
{
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
        ns::config cfg(argc == 1 ? (parser_name + ".conf").c_str() : argv[1]);
        ns::proceed_parser(can_run);
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

#ifndef PIP_HERE

void (*term_signal_func)(int);
#include "../makoa/imports.hpp"

extern "C"
{
    void create_import(hole_importer* i, simple_log* sl)
    {
        log_set(sl);

        struct parser 
        {
            volatile bool& can_run;
            ns::config cfg;

            parser(volatile bool& can_run, str_holder conf) : can_run(can_run),
                cfg(conf.empty() ? (parser_name + ".conf").c_str() : conf.begin())
            {
                if(cfg.push != "local_import")
                {
                    mlog() << cfg.exchange_id << " push changed from " << cfg.push << " to local_import";
                    cfg.push = "local_import";
                }
            }
            void start()
            {
                ns::proceed_parser(can_run);
            }
        };

        auto init = [](volatile bool& can_run, char_cit params)->void*
        {
            return new parser(can_run, _str_holder(params));
        };
        auto destroy = [](void* c)
        {
            delete ((parser*)c);
        };
        auto start = [](void* c, void*)
        {
            ((parser*)c)->start();
        };

        i->init = init;
        i->destroy = destroy;
        i->start = start;
    }
}

#endif

