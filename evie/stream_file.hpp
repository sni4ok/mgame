/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "string.hpp"
#include "mfile.hpp"

static const uint32_t b_stream_size = 1024 * 1024;
typedef buf_stream_fixed<b_stream_size> b_stream;

struct stream_file : b_stream
{
    mstring fname;

    stream_file(str_holder f, bool trunc) : fname(f) {
        write_file(fname.c_str(), begin(), size(), trunc);
    }
    void flush(bool force)
    {
        if(force || (size() > b_stream_size / 2))
        {
            write_file(fname.c_str(), begin(), size(), false);
            clear();
        }
    }
    ~stream_file() {
        write_file(fname.c_str(), begin(), size(), false);
    }
};

struct flush
{
    bool force;
};

static const flush flush_file{false};
static const flush flush_file_force{true};

inline stream_file& operator<<(buf_stream& b, flush f)
{
    stream_file& s = static_cast<stream_file&>(b);
    s.flush(f.force);
    return s;
}

struct csv_file : stream_file
{
    char sep;

    csv_file(str_holder fname, bool trunc, char sep = ',') : stream_file(fname, trunc), sep(sep)
    {
    }
    
    void write_args()
    {
    }

    template<typename type, typename ... types>
    void write_args(const type& t, types ... args)
    {
        (*this) << sep << t;
        write_args(args...);
    }

    template<typename type, typename ... types>
    void operator()(const type& t, types ... args)
    {
        (*this) << t;
        write_args(args...);
        (*this) << endl;
    }
};

