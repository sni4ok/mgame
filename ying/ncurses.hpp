/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/utils.hpp"
#include "../evie/mlog.hpp"

#include <curses.h>

#include <sys/ioctl.h>

struct ncurses_err
{
    ncurses_err()
    {
    }
    ncurses_err& operator=(int ret)
    {
        if(ret != OK)
            throw str_exception("ncurses error");
        return *this;
    }
    ncurses_err& operator&(int ret)
    {
        if(ret != OK)
            throw str_exception("ncurses error");
        return *this;
    }

    ncurses_err(const ncurses_err&) = delete;
};

class window
{
    static void end_win(WINDOW*)
    {
        endwin();
    }
    unique_ptr<WINDOW, end_win> w;
    ncurses_err e;

    long log_par;

    window(const window&) = delete;

public:
    u32 rows, cols;
    mvector<char> blank_row;

    window() : w(initscr())
    {
        if(!w)
            throw str_exception("initscr, error");

        log_par = log_params();
        log_params() ^= mlog::always_cout;

        resize();

        e = nodelay(w.get(), TRUE);
        e = noecho();
        e = keypad(stdscr, TRUE);
        e = cbreak();
    }
    void resize()
    {
        winsize size;
        if(ioctl(0, TIOCGWINSZ, (char*)&size) < 0)
            throw str_exception("TIOCGWINSZ, error");
        
        rows = size.ws_row;
        cols = size.ws_col;

        blank_row.resize(cols);
        fill(blank_row.begin(), blank_row.end(), ' ');

        e = wresize(w.get(), rows, cols);
    }
    void clear()
    {
        wclear(w.get());
    }
    ~window()
    {
        log_params() = log_par;
    }
    operator WINDOW*()
    {
        return w.get();
    }
};

