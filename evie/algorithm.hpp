/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "pair.hpp"
#include "str_holder.hpp"

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

template<class it1, class it2>
it1 search(it1 first1, it1 last1, it2 first2, it2 last2)
{
    for(;;)
    {
        it1 i1 = first1;
        for(it2 i2 = first2; ; ++i1, ++i2)
        {
            if(i2 == last2)
                return first1;
            if(i1 == last1)
                return last1;
            if(*i1 != *i2)
                break;
        }
        ++first1;
    }
}

template<class It, class T, class Pr>
It lower_bound(It first, It last, const T& val, Pr pred)
{
    int64_t count = last - first;
    while(count > 0)
    {
        int64_t step = count / 2;
        It mid = first + step;
        if(pred(*mid, val))
            first = ++mid, count -= step + 1;
        else
            count = step;
    }
    return first;
}

template <class It, class T, class Pr>
It upper_bound(It first, It last, const T& val, Pr pred)
{
  int64_t count = last - first;
  while(count > 0)
  {
    int64_t step = count / 2;
    It mid = first + step;
    if(!pred(val, *mid))
        first = ++mid, count -= step + 1;
    else
        count = step;
  }
  return first;
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
type min(type l, type r)
{
    if(l < r)
        return l;
    return r;
}

template<typename type>
type max(type l, type r)
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
void fill(iterator f, iterator t, const type& v)
{
    for(; f != t; ++f)
        *f = v;
}

template<typename iterator, typename type>
type accumulate(iterator f, iterator t, const type& v)
{
    type ret = v;
    for(; f != t; ++f)
        ret += *f;
    return ret;
}

template<typename iterator, typename func>
void for_each(iterator f, iterator t, const func& fun)
{
    for(; f != t; ++f)
        fun(*f);
}

template<typename iterator, typename cont>
void copy_back(iterator f, iterator t, cont& c)
{
    for(; f != t; ++f)
        c.push_back(*f);
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

