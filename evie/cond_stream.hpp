/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "new.hpp"

template<typename s1, typename s2>
struct conditional_stream : ios_base
{
    bool __s2;
    char buf[sizeof(s1) > sizeof(s2) ? sizeof(s1) : sizeof(s2)];

    template<typename p1, typename p2>
    conditional_stream(bool __s2, const p1& _p1, const p2& _p2) : __s2(__s2)
    {
        if(__s2)
            new(buf) s2(_p2);
        else
            new(buf) s1(_p1);
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

