/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "decimal.hpp"

template<typename type>
inline type atomic_add(type& v, type value, int m = __ATOMIC_RELAXED)
{
    return __atomic_add_fetch(&v, value, m);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_add(type& v, type value, int m = __ATOMIC_RELAXED)
{
    return {atomic_add(v.value, value.value, m)};
}

template<typename type>
inline type atomic_sub(type& v, type value, int m = __ATOMIC_RELAXED)
{
    return __atomic_sub_fetch(&v, value, m);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_sub(type& v, type value, int m = __ATOMIC_RELAXED)
{
    return {atomic_sub(v.value, value.value, m)};
}

template<typename type>
inline void atomic_store(type& v, type value, int m = __ATOMIC_RELAXED)
{
    __atomic_store_n(&v, value, m);
}

template<typename type>
requires(is_decimal<type>)
inline void atomic_store(type& v, type value, int m = __ATOMIC_RELAXED)
{
    atomic_store(v.value, value.value, m);
}

template<typename type>
inline type atomic_load(type& v, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&v, m);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_load(type& v, int m = __ATOMIC_RELAXED)
{
    return {atomic_load(v.value, m)};
}

template<typename type>
inline bool atomic_compare_exchange(type& v, type from, type to,
    int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

template<typename type>
requires(is_decimal<type>)
inline bool atomic_compare_exchange(type& v, type from, type to,
    int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return atomic_compare_exchange(v.value, from.value, to.value, sm, fm);
}

template<typename type>
type atomic_exchange(type* ptr, type val, int m = __ATOMIC_RELAXED)
{
    return __atomic_exchange_n(ptr, val, m);
}

