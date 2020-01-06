/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/mlog.hpp"
#include "evie/utils.hpp"
#include "evie/profiler.hpp"
#include "evie/config.hpp"

#include <csignal>

volatile bool parser_can_run = true;

void term_signal(int sign)
{
    mlog() << "Termination signal received: " << sign;
    parser_can_run = false;
}

void on_signal(int sign)
{
    mlog() << "Signal received: " << sign;
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
    log_raii li(argc == 1 ? (parser_name + ".log") : get_log_name(argv[1]), mlog::always_cout | mlog::info);
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

