/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_STRING_HPP
#define EVIE_STRING_HPP

#include "vector.hpp"

inline void my_fast_copy(char_cit from, uint64_t size, char_it out)
{
    memcpy(out, from, size);
}
inline void my_fast_move(char_cit from, uint64_t size, char_it out)
{
    memmove(out, from, size);
}
inline void my_fast_copy(char_cit from, char_cit to, char_it out)
{
    memcpy(out, from, to - from);
}
inline void my_fast_move(char_cit from, char_cit to, char_it out)
{
    memmove(out, from, to - from);
}

struct buf_stream
{
    char_it from, cur, to;

    buf_stream(char_it from, char_it to) : from(from), cur(from), to(to)
    {
    }

    template<uint64_t size>
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
    uint64_t size() const
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
    void resize(uint64_t new_sz)
    {
        uint64_t cur_sz = size();
        if(new_sz > cur_sz)
            check_size(new_sz - cur_sz);
        cur = from + new_sz;
    }
    void write(char_cit v, uint64_t s)
    {
        if(cur + s > to) [[unlikely]]
            throw str_exception("buf_stream::write() overloaded");
        my_fast_copy(v, s, cur);
        cur += s;
    }
    buf_stream& operator<<(char c)
    {
        check_size(1);
        *cur = c;
        ++cur;
        return *this;
    }
    void check_size(uint64_t delta) const
    {
        if(cur + delta > to) [[unlikely]]
            throw str_exception("buf_stream::check_size() overloaded");
    }

    template<typename type>
    requires(is_integral<type>::value)
    buf_stream& operator<<(type t)
    {
        check_size(my_cvt::atoi_size<type>::value);
        cur += my_cvt::itoa(cur, t);
        return *this;
    }
    buf_stream& operator<<(double d)
    {
        check_size(30);
        cur += my_cvt::dtoa(cur, d);
        return *this;
    }
};

template<uint32_t stack_sz>
struct buf_stream_fixed : buf_stream
{
    char buf[stack_sz];

    buf_stream_fixed() : buf_stream(buf)
    {
    }
};

typedef buf_stream_fixed<16 * 1024> my_stream;

struct es
{
    my_stream s;

    template<typename type>
    es& operator%(const type& t)
    {
        s << t;
        return *this;
    }
    operator str_holder() const
    {
        return s.str();
    }
};

void throw_system_failure(str_holder msg);
void cout_write(str_holder str, bool flush = true);
void cerr_write(str_holder str, bool flush = true);

template<void (*func)(str_holder, bool)>
struct ostream
{
    my_stream s;

    template<typename type>
    ostream& operator<<(const type& t)
    {
        s << t;
        return *this;
    }
    void write(char_cit v, uint32_t size)
    {
        s.write(v, size);
    }
    ~ostream()
    {
        func(s.str(), true);
    }
};

typedef ostream<cout_write> cout;
typedef ostream<cerr_write> cerr;

#endif

