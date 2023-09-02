/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/profiler.hpp"

#include <csignal>

volatile bool can_run = true;
void (*on_term_signal)() = nullptr;

extern "C"
{
    void term_signal(int sign)
    {
        mlog() << "Termination signal received: " << sign;
        can_run = false;
        if(on_term_signal)
            on_term_signal();
    }

    void on_signal(int sign)
    {
        mlog() << "Signal received: " << sign;
    }

    void on_usr_signal(int sign)
    {
        mlog(mlog::critical) << "Signal usr received: " << sign;
        profilerinfo::instance().print();
    }
}

void init_signals(void (*term_signal_func)() = nullptr)
{
    std::signal(SIGTERM, &term_signal);
    std::signal(SIGINT, &term_signal);
    std::signal(SIGHUP, &on_signal);
    std::signal(SIGPIPE, &on_signal);
    std::signal(SIGUSR1, &on_usr_signal);
    if(term_signal_func)
        on_term_signal = term_signal_func;
}

