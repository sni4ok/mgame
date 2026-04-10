#pragma once

#ifndef DECIMAL_HPP
#define DECIMAL_HPP

#include "type_traits.hpp"

template<typename type>
concept is_decimal = is_class_v<type> && requires(type* t)
{
    t->exponent;
    t->value;
};

template<u32 v>
struct pow10
{
    static constexpr u64 value = pow10<v - 1>::value * 10;
};

template<>
struct pow10<0>
{
    static constexpr u64 value = 1;
};

template<u32 v>
inline constexpr u64 pow10_v = pow10<v>::value;

template<typename type>
requires(is_decimal<type>)
inline constexpr i64 frac()
{
    static_assert(type::exponent <= 0);
    return pow10_v<-type::exponent>;
}

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
        if constexpr(frac<to>() >= frac<from>())
            return {v.value * (frac<to>() / frac<from>())};
        else
            return {v.value / (frac<from>() / frac<to>())};
    }
    else
        return {v * frac<to>()};
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type div_int(type p, i64 value)
{
    return {p.value / value};
}

template<typename type>
requires(is_decimal<type>)
inline constexpr type mul_int(type p, i64 value)
{
    return {p.value * value};
}

template<typename type>
inline constexpr double to_double(type p)
{
    if constexpr(is_decimal<type>)
        return double(p.value) / frac<type>();
    else
        return double(p);
}

template<typename type>
inline constexpr i64 to_int(type p)
{
    if constexpr(is_decimal<type>)
        return p.value / frac<type>();
    else
        return p;
}

struct p2 
{
    static const i64 exponent = -2;
    i64 value;
};

#endif

