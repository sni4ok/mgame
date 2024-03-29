/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "pair.hpp"
#include "str_holder.hpp"

#include <utility> //for std::move

template<typename iterator, typename type>
iterator find(iterator from, iterator to, const type& value)
{
    for(; from != to; ++from)
    {
        if(*from == value)
            return from;
    }
    return from;
}

template<typename iterator, typename compare>
iterator find_if(iterator from, iterator to, compare cmp)
{
    for(; from != to; ++from)
    {
        if(cmp(*from))
            return from;
    }
    return from;
}

template<typename it1, typename it2>
it1 search(it1 from_1, it1 to_1, it2 from_2, it2 to_2)
{
    for(;;)
    {
        it1 i1 = from_1;
        for(it2 i2 = from_2; ; ++i1, ++i2)
        {
            if(i2 == to_2)
                return from_1;
            if(i1 == to_1)
                return to_1;
            if(*i1 != *i2)
                break;
        }
        ++from_1;
    }
}

template<typename iterator, typename type, typename pr>
iterator lower_bound(iterator from, iterator to, const type& val, pr pred)
{
    int64_t count = to - from;
    while(count > 0)
    {
        int64_t step = count / 2;
        iterator mid = from + step;
        if(pred(*mid, val))
            from = ++mid, count -= step + 1;
        else
            count = step;
    }
    return from;
}

template<typename iterator, typename type, typename pr>
iterator upper_bound(iterator from, iterator to, const type& val, pr pred)
{
  int64_t count = to - from;
  while(count > 0)
  {
    int64_t step = count / 2;
    iterator mid = from + step;
    if(!pred(val, *mid))
        from = ++mid, count -= step + 1;
    else
        count = step;
  }
  return from;
}

template<typename iterator, typename type>
void replace(iterator from, iterator to, const type& f, const type& t)
{
    for(; from != to; ++from)
    {
        if(*from == f)
            *from = t;
    }
}

struct any_of
{
    const char *b, *e;

    any_of(const char* b, const char* e) : b(b), e(e)
    {
    }
    constexpr bool operator()(char c) const
    {
        for(const char* i = b; i != e; ++i)
        {
            if(*i == c)
                return true;
        }
        return false;
    }
};

template<uint64_t size>
any_of is_any_of(const char (&str)[size])
{
    return any_of(str, str + size - 1);
}

template<typename type>
constexpr type min(type l, type r)
{
    if(l < r)
        return l;
    return r;
}

template<typename type>
constexpr type max(type l, type r)
{
    if(l > r)
        return l;
    return r;
}

template<typename type>
struct less
{
    constexpr bool operator()(const type& l, const type& r) const
    {
        return l < r;
    }
};

template<typename type>
struct greater
{
    constexpr bool operator()(const type& l, const type& r) const
    {
        return l > r;
    }
};

inline bool equal(char_cit b1, char_cit e1, char_cit b2)
{
    return !(memcmp(b1, b2, e1 - b1));
}

template<typename iterator, typename type>
void fill(iterator from, iterator to, const type& v)
{
    for(; from != to; ++from)
        *from = v;
}

template<typename iterator, typename type>
type accumulate(iterator from, iterator to, const type& v)
{
    type ret = v;
    for(; from != to; ++from)
        ret += *from;
    return ret;
}

template<typename iterator, typename func>
void for_each(iterator from, iterator to, const func& fun)
{
    for(; from != to; ++from)
        fun(*from);
}

template<typename iterator, typename cont>
void copy_back(iterator from, iterator to, cont& c)
{
    for(; from != to; ++from)
        c.push_back(*from);
}

template<typename ifrom, typename ito>
void copy(ifrom from, ifrom to, ito out)
{
    for(; from != to; ++from, ++out)
        *out = *from;
}

template<bool min, typename iterator, typename compare>
iterator min_element_impl(iterator from, iterator to, compare cmp)
{
    if(from == to)
        return from;
    iterator it = from;
    ++from;
    for(; from != to; ++from)
    {
        if(min)
        {
            if(cmp(*from, *it))
                it = from;
        }
        else
        {
            if(cmp(*it, *from))
                it = from;
        }
    }
    return it;
}

template<typename iterator, typename compare>
iterator min_element(iterator from, iterator to, compare cmp)
{
    return min_element_impl<true>(from, to, cmp);
}

template<typename iterator, typename compare>
iterator max_element(iterator from, iterator to, compare cmp)
{
    return min_element_impl<false>(from, to, cmp);
}

template<typename iterator, typename compare>
pair<iterator, iterator> minmax_element(iterator from, iterator to, compare cmp)
{
    return {min_element(from, to, cmp), max_element(from, to, cmp)};
}

template<typename iterator, typename pr>
iterator remove_if(iterator from, iterator to, pr p)
{
    from = find_if(from, to, p);
    if(from != to)
        for(iterator i = from; ++i != to;)
            if(!p(*i))
                *from++ = std::move(*i);
    return from;
}

