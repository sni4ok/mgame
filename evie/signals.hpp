/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

extern volatile bool can_run;

struct signals_holder
{
    typedef void (*signal_f)(int);
    signals_holder(signal_f term_signal_func = nullptr, signal_f usr_signal_func = nullptr);
    ~signals_holder();
};

