/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"
#include "cvt.hpp"

struct buf_stream : ios_base
{
    char_it from, cur, to;

    buf_stream(char_it from, char_it to) : from(from), cur(from), to(to)
    {
    }

    template<u64 size>
    buf_stream(char (&v)[size]) : from(v), cur(from), to(from + size)
    {
    }
    char_cit begin() const
    {
        return from;
    }
    char_cit end() const
    {
        return cur;
    }
    u64 size() const
    {
        return cur - from;
    }
    str_holder str() const
    {
        return str_holder(begin(), size());
    }
    char_cit c_str()
    {
        check_size(1);
        *cur = char();
        return from;
    }
    bool empty() const
    {
        return cur == from;
    }
    void clear()
    {
        cur = from;
    }
    void resize(u64 new_sz)
    {
        u64 cur_sz = size();
        if(new_sz > cur_sz)
            check_size(new_sz - cur_sz);
        cur = from + new_sz;
    }
    void write(char_cit v, u64 s)
    {
        if(s > u64(to - cur)) [[unlikely]]
            throw str_exception("buf_stream::write() overloaded");
        memcpy(cur, v, s);
        cur += s;
    }
    void check_size(u64 delta) const
    {
        if(delta > u64(to - cur)) [[unlikely]]
            throw str_exception("buf_stream::check_size() overloaded");
    }
    template<typename type>
    void write_numeric(type t)
    {
        check_size(atoi_size<type>::value);
        cur += itoa(cur, t);
    }
};

template<u32 stack_sz>
struct buf_stream_fixed : buf_stream
{
    char buf[stack_sz];

    buf_stream_fixed() : buf_stream(buf)
    {
    }
};

typedef buf_stream_fixed<16 * 1024> mstream;

struct es : mstream
{
    template<typename type>
    es& operator%(const type& t)
    {
        *this << t;
        return *this;
    }
    operator str_holder() const
    {
        return str();
    }
};

void throw_system_failure(str_holder msg);
void cout_write(str_holder str, bool flush = true);
void cerr_write(str_holder str, bool flush = true);

template<void (*func)(str_holder, bool)>
struct ostream : mstream
{
    bool print_endl;

    ostream(bool print_endl = true) : print_endl(print_endl)
    {
    }
    ~ostream()
    {
        if(print_endl)
            *this << endl;
        func(str(), true);
    }
};

typedef ostream<cout_write> cout;
typedef ostream<cerr_write> cerr;

