/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <cstdint>

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
    constexpr bool operator()(const type& l, const type& r)
    {
        return l < r;
    }
};

inline bool equal(char_cit b1, char_cit e1, char_cit b2)
{
    return !(memcmp(b1, b2, e1 - b1));
}

