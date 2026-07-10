/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "tuple.hpp"
#include "mstring.hpp"
#include "string.hpp"

template<typename tuple>
mstring tuple_to_string(const tuple& v)
{
    mstring ret;
    ret.resize(256);
    buf_stream str(&ret[0], &ret[0] + ret.size());
    write_tuple_elem<0>(str, v);
    ret.resize(str.size());
    return ret;
}

template<int>
auto read_csv_impl(char_cit, char_cit, char)
{
    return tuple<>();
}

template<int idx, typename type, typename ... types>
auto read_csv_impl(char_cit& i, char_cit e, char sep)
{
    char_cit te, t;
    if((e - i) >= 2 && *i == '\"')
    {
        ++i;
        te = find(i, e, '\"');
        if(te == e)
            throw_exception("read_csv failure: ", {i, e});
        t = te + 1;
    }
    else
    {
        te = find(i, e, sep);
        t = te;
    }
    type v = lexical_cast<type>(i, te);
    i = t;
    if(i != e)
        ++i;

    auto ret = tuple_cat(make_tuple(v), read_csv_impl<idx + 1, types...>(i, e, sep));
    if(i != e)
        throw_exception("read_csv not complete: ", {i, e});
    return ret;
}

template<typename ... types, typename func>
void read_csv(func f, str_holder s, char sep = ',', bool skip_empty_lines = true)
{
    char_cit i = s.begin(), e = s.end();
    while(i != e)
    {
        char_cit t = find(i, e, endl);
        if(t == e)
            throw_exception("read_csv no endl found");
        if(*i == '#' || (skip_empty_lines && i == t))
        {
            i = t + 1;
            continue;
        }
        auto v = read_csv_impl<0, types...>(i, t, sep);
        f(v);
        i = t + 1;
    }
}

