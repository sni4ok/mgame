/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "stdlib.hpp"

template<typename t>
struct is_trivially_destructible
{
#ifdef CLANG_COMPILER
    static const bool value = __is_trivially_destructible(t);
#else
    static const bool value = __has_trivial_destructor(t);
#endif
};

template<typename t>
struct remove_const
{
    typedef t type;
};

template<typename t>
struct remove_const<const t>
{
    typedef t type;
};

template<typename ifrom, typename ito>
constexpr void __copy(ifrom from, ifrom to, ito out)
{
    for(; from != to; ++from, ++out)
        *out = *from;
}

template<typename ifrom, typename ito>
constexpr void copy(ifrom from, ifrom to, ito out)
{
    __copy(from, to, out);
}

template<typename type>
constexpr void copy(type* from, type* to, typename remove_const<type>::type* out)
    requires(is_trivially_destructible<type>::value)
{
    if consteval
    {
        __copy(from, to, out);
    }
    else
    {
        memcpy(out, from, (to - from) * sizeof(type));
    }
}

template<typename t>
struct reverse_iter
{
    t* ptr;

    reverse_iter& operator++()
    {
        --ptr;
        return *this;
    }
    reverse_iter operator+(i64 c) const
    {
        return {ptr - c};
    }
    reverse_iter operator-(i64 c) const
    {
        return {ptr + c};
    }
    i64 operator-(const reverse_iter& r) const
    {
        return r.ptr - ptr;
    }
    t& operator*()
    {
        return *ptr;
    }
    t* operator->()
    {
        return ptr;
    }
    bool operator==(const reverse_iter& r) const
    {
        return ptr == r.ptr;
    }
};

template<typename t>
struct remove_reference
{
    typedef t type;
};

template<typename t>
struct remove_reference<t&>
{
    typedef t type;
};

template<typename t>
struct remove_reference<t&&>
{
    typedef t type;
};

template<typename type>
[[nodiscard]] constexpr remove_reference<type>::type&& move(type&& t) noexcept
{
    return static_cast<remove_reference<type>::type&&>(t);
}

