/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "parse.hpp"

#include "evie/mlog.hpp"
#include "evie/utils.hpp"
#include "evie/profiler.hpp"

#include <csignal>

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
    std::signal(SIGTERM, &term_signal);
    std::signal(SIGINT, &term_signal);
    std::signal(SIGHUP, &on_signal);
    if(argc > 2) {
        std::cout << "Usage: ./bitfinex [config file]" << std::endl;
        return 1;
    }
    log_raii li(argc == 1 ? "bitfinex.log" : get_log_name(argv[1]), mlog::always_cout | mlog::info);
    profilerinfo pff_info;
    try {
        mlog() << "bitfinex started";
        print_init(argc, argv);
        config cfg(argc == 1 ? "bitfinex.conf" : argv[1]);
        proceed_bitfinex(can_run);

    } catch(std::exception& e) {
        //TODO: implement reconnections for proceed_bitfinex

        mlog(mlog::error) << "bitfinex ended with " << e;
        return 1;
    }
    mlog() << "bitfinex successfully ended";
    return 0;
}

