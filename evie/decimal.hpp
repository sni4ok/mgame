#pragma once

#ifndef DECIMAL_HPP
#define DECIMAL_HPP

#include "type_traits.hpp"

template<typename type>
concept is_decimal = is_class_v<type> && requires(type* t)
{
    t->frac;
    t->exponent;
};

template<typename type>
requires(is_decimal<type>)
inline constexpr bool operator<(type l, type r)
{
    return l.value < r.value;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr bool operator>(type l, type r)
{
    return l.value > r.value;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr bool operator==(type l, type r)
{
    return l.value == r.value;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr bool operator<=(type l, type r)
{
    return l.value <= r.value;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr bool operator>=(type l, type r)
{
    return l.value >= r.value;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type operator+(type l, type r)
{
    return {l.value + r.value};
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type operator-(type l, type r)
{
    return {l.value - r.value};
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type& operator+=(type& l, type r)
{
    l.value += r.value;
    return l;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type& operator-=(type& l, type r)
{
    l.value -= r.value;
    return l;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type operator%(type l, type r)
{
    return {l.value % r.value};
}

template<typename type>
requires(is_decimal<type>)
inline constexpr bool operator!(type v)
{
    return !v.value;
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type operator-(type v)
{
    return {-v.value};
}

template<typename to, typename from>
requires(is_decimal<to>)
inline constexpr to to_decimal(from v)
{
    if constexpr(is_decimal<from>)
    {
        if constexpr(to::frac >= from::frac)
            return {v.value * (to::frac / from::frac)};
        else
            return {v.value / (from::frac / to::frac)};
    }
    else
        return {v * to::frac};
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type div_int(type p, int64_t value)
{
    return {p.value / value};
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type mul_int(type p, int64_t value)
{
    return {p.value * value};
}

template<typename type>
inline constexpr double to_double(type p)
{
    if constexpr(is_decimal<type>)
        return double(p.value) / type::frac;
    else
        return double(p);
}

template<typename type>
inline constexpr int64_t to_int(type p)
{
    if constexpr(is_decimal<type>)
        return p.value / type::frac;
    else
        return p;
}

struct p2 
{
    static const int64_t exponent = -2;
    static const int64_t frac = 100;
    int64_t value;
};

#endif

