/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_ALGORITHM_HPP
#define EVIE_ALGORITHM_HPP

#include "pair.hpp"
#include "str_holder.hpp"
#include "type_traits.hpp"

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

template<typename iterator, typename type, typename compare = less<type> >
iterator lower_bound(iterator from, iterator to, const type& val, compare cmp = compare())
{
    int64_t count = to - from;
    while(count > 0)
    {
        int64_t step = count / 2;
        iterator mid = from + step;
        if(cmp(*mid, val))
            from = ++mid, count -= step + 1;
        else
            count = step;
    }
    return from;
}

template<typename iterator, typename type, typename compare = less<type> >
iterator upper_bound(iterator from, iterator to, const type& val, compare cmp = compare())
{
  int64_t count = to - from;
  while(count > 0)
  {
    int64_t step = count / 2;
    iterator mid = from + step;
    if(!cmp(val, *mid))
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
void for_each(iterator from, iterator to, func fun)
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
constexpr void copy(ifrom from, ifrom to, ito out)
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

template<typename iterator, typename compare = less<remove_pointer_t<iterator> > >
iterator min_element(iterator from, iterator to, compare cmp = compare())
{
    return min_element_impl<true>(from, to, cmp);
}

template<typename iterator, typename compare = less<remove_pointer_t<iterator> > >
iterator max_element(iterator from, iterator to, compare cmp = compare())
{
    return min_element_impl<false>(from, to, cmp);
}

template<typename iterator, typename compare = less<remove_pointer_t<iterator> > >
pair<iterator, iterator> minmax_element(iterator from, iterator to, compare cmp = compare())
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
                *from++ = move(*i);
    return from;
}

template<typename type>
struct ref
{
    type* t;

    ref(type& t) : t(&t)
    {
    }
    operator type&()
    {
        return *t;
    }
    template<typename ... types>
    auto operator()(types ... args)
    {
        return (*t)(args...);
    }
};

struct forward_iterator_tag
{
};

template<typename iterator>
requires requires { is_same_v<typename iterator::iterator_category, forward_iterator_tag>; }
iterator advance(iterator it, uint64_t size)
{
    for(uint32_t i = 0; i != size; ++i, ++it)
        ;
    return it;
}

template<typename iterator>
iterator advance(iterator it, uint64_t size)
{
    it += size;
    return it;
}

#endif

