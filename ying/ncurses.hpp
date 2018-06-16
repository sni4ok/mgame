/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <memory>

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
            throw std::runtime_error("ncurses error");
        return *this;
    }
    ncurses_err& operator&(int ret)
    {
        if(ret != OK)
            throw std::runtime_error("ncurses error");
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
	std::unique_ptr<WINDOW, decltype(&end_win)> w;
    ncurses_err e;

    long log_par;

public:
    uint32_t rows, cols;
    std::vector<char> blank_row;

    window() : w(initscr(), &end_win)
    {
        log_par = log_params();
        log_params() ^= mlog::always_cout;
        
        winsize size;
        if(ioctl(0, TIOCGWINSZ, (char *)&size) < 0)
            throw std::runtime_error("TIOCGWINSZ error");
        
        rows = size.ws_row;
        cols = size.ws_col;

        if(!w)
            throw std::runtime_error("initscr() error");
        e = wresize(w.get(), rows, cols);
        e = nodelay(w.get(), TRUE);

		e = noecho();
        e = keypad(stdscr, TRUE);
        e = cbreak();
        
        blank_row.resize(cols, ' ');
    }
    void clear()
    {
        for(uint32_t i = 0; i != rows - 1; ++i)
            e = mvwaddnstr(w.get(), i, 0, &blank_row[0], blank_row.size());
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

