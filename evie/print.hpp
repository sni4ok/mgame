/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "str_holder.hpp"

struct print_default
{
    template<typename stream, typename type>
    stream& operator()(stream& s, const type& v) const
    {
        if constexpr(__is_same(type, const char*))
            return s << _str_holder(v);
        else
            return s << v;
    }
};

