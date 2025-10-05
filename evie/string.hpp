/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_STRING_HPP
#define EVIE_STRING_HPP

#include "vector.hpp"

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
        check_size(cvt::atoi_size<type>::value);
        cur += cvt::itoa(cur, t);
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

template<typename s1, typename s2>
struct conditional_stream : ios_base
{
    bool __s2;
    char buf[sizeof(s1) > sizeof(s2) ? sizeof(s1) : sizeof(s2)];

    template<typename p1, typename p2>
    conditional_stream(bool __s2, const p1& _p1, const p2& _p2) : __s2(__s2)
    {
        if(__s2)
            new (buf) s2(_p2);
        else
            new (buf) s1(_p1);
    }
    ~conditional_stream()
    {
        if(__s2)
            ((s2*)(buf))->~s2();
        else
            ((s1*)(buf))->~s1();
    }
    void write(char_cit v, u64 s)
    {
        if(__s2)
            ((s2*)(buf))->write(v, s);
        else
            ((s1*)(buf))->write(v, s);
    }
    template<typename type>
    void write_numeric(type v)
    {
        if(__s2)
            ((s2*)(buf))->write_numeric(v);
        else
            ((s1*)(buf))->write_numeric(v);
    }
};

#endif

