/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <stddef.h>

template<typename f, typename s>
struct pair
{
    f first;
    s second;

    f begin() const
    {
        return first;
    }
    f end() const
    {
        return second;
    }
    static constexpr size_t tuple_size()
    {
        return 2;
    }
    template<unsigned i>
    auto& get()
    {
        if constexpr(i == 0)
            return first;
        else if constexpr(i == 1)
            return second;
        else
            static_assert(false);
    }
    template<unsigned i>
    const auto& get() const
    {
        return const_cast<pair*>(this)->template get<i>();
    }
};

template<typename f, typename s>
bool operator<(const pair<f, s>& l, const pair<f, s>& r)
{
    if(l.first == r.first)
        return l.second < r.second;
    return l.first < r.first;
}

template<typename f, typename s>
bool operator!=(const pair<f, s>& l, const pair<f, s>& r)
{
    return l.first != r.first || l.second != r.second;
}

template<typename f, typename s>
bool operator==(const pair<f, s>& l, const pair<f, s>& r)
{
    return l.first == r.first && l.second == r.second;
}

template<typename stream, typename f, typename s>
stream& operator<<(stream& str, const pair<f, s>& p)
{
    str << p.first << ',' << p.second;
    return str;
}

template<unsigned i, typename f, typename s>
const auto& get(const ::pair<f, s>& p)
{
    return p.template get<i>();
}

template<unsigned i, typename f, typename s>
auto& get(::pair<f, s>& p)
{
    return p.template get<i>();
}

