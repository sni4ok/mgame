/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "parse.hpp"

#include "evie/log.hpp"
#include "evie/utils.hpp"

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
        std::cout << "Usage: ./bitfinex [config file]" << std::endl;
        return 1;
    }
    log_init li(argc == 1 ? "bitfinex.log" : get_log_name(argv[1]), mlog::cout | mlog::info);
    try {
        mlog(mlog::cout) << "bitfinex started";
        print_init(argc, argv);
        config cfg(argc == 1 ? "bitfinex.conf" : argv[1]);
        proceed_bitfinex(can_run);

    } catch(std::exception& e) {
        //TODO: implement reconnections for proceed_bitfinex

        mlog(mlog::error | mlog::cout) << "bitfinex ended with " << e;
        return 1;
    }
    mlog(mlog::cout) << "bitfinex successfully ended";
    return 0;
}

