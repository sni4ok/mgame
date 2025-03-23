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
constexpr inline bool operator<(type l, type r)
{
    return l.value < r.value;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline bool operator>(type l, type r)
{
    return l.value > r.value;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline bool operator!=(type l, type r)
{
    return l.value != r.value;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline bool operator==(type l, type r)
{
    return l.value == r.value;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline bool operator<=(type l, type r)
{
    return l.value <= r.value;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline bool operator>=(type l, type r)
{
    return l.value >= r.value;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline type operator+(type l, type r)
{
    return {l.value + r.value};
}

template<typename type>
requires(is_decimal<type>)
constexpr inline type operator-(type l, type r)
{
    return {l.value - r.value};
}

template<typename type>
requires(is_decimal<type>)
constexpr inline type& operator+=(type& l, type r)
{
    l.value += r.value;
    return l;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline type& operator-=(type& l, type r)
{
    l.value -= r.value;
    return l;
}

template<typename type>
requires(is_decimal<type>)
constexpr inline type operator%(type l, type r)
{
    return {l.value % r.value};
}

template<typename type>
requires(is_decimal<type>)
constexpr bool operator!(type v)
{
    return !v.value;
}

template<typename type>
requires(is_decimal<type>)
constexpr type operator-(type v)
{
    return {-v.value};
}

#endif

