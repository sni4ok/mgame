/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/profiler.hpp"

#include <csignal>

volatile bool parser_can_run = true;

void (*on_term_signal)() = nullptr;

extern "C"
{
    void term_signal(int sign)
    {
        mlog() << "Termination signal received: " << sign;
        parser_can_run = false;
        if(on_term_signal)
            on_term_signal();
    }

    void on_signal(int sign)
    {
        mlog() << "Signal received: " << sign;
    }
}

int parser_main(int argc, char** argv, const std::string& parser_name, void (*proceed)(volatile bool&))
{
    std::signal(SIGTERM, &term_signal);
    std::signal(SIGINT, &term_signal);
    std::signal(SIGHUP, &on_signal);
    std::signal(SIGPIPE, &on_signal);
    if(argc > 2) {
        std::cout << "Usage: ./" << parser_name << " [config file]" << std::endl;
        return 1;
    }
    auto log = log_init(argc == 1 ? (parser_name + ".log").c_str() : get_log_name(argv[1]).c_str(), mlog::always_cout | mlog::info);
    profilerinfo pff_info;
    try {
        mlog() << parser_name << " started";
        print_init(argc, argv);
        config cfg(argc == 1 ? (parser_name + ".conf").c_str() : argv[1]);
        proceed(parser_can_run);

    } catch(std::exception& e) {
        mlog(mlog::error) << parser_name << " ended with " << e;
        return 1;
    }
    mlog() << parser_name << " successfully ended";
    return 0;
}

