/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "type_traits.hpp"
#include "str_holder.hpp"

struct print_default
{
    template<typename stream, typename type>
    stream& operator()(stream& s, const type& v) const
    {
        if constexpr(is_same_v<type, const char*>)
            return s << _str_holder(v);
        else
            return s << v;
    }
};

