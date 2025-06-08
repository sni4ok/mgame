/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "decimal.hpp"

#include <cstdint>

inline uint64_t atomic_add(uint64_t& v, uint64_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int64_t atomic_add(int64_t& v, int64_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline uint32_t atomic_add(uint32_t& v, uint32_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int32_t atomic_add(int32_t& v, int32_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_add(type& v, type value)
{
    return {atomic_add(v.value, value.value)};
}

inline uint64_t atomic_sub(uint64_t& v, uint64_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int64_t atomic_sub(int64_t& v, int64_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

inline uint32_t atomic_sub(uint32_t& v, uint32_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int32_t atomic_sub(int32_t& v, int32_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_sub(type& v, type value)
{
    return {atomic_sub(v.value, value.value)};
}

inline void atomic_store(uint64_t& v, uint64_t value)
{
    __atomic_store_n(&v, value, __ATOMIC_RELAXED);
}

inline void atomic_store(int64_t& v, int64_t value)
{
    __atomic_store_n(&v, value, __ATOMIC_RELAXED);
}

inline void atomic_store(uint32_t& v, uint32_t value)
{
    __atomic_store_n(&v, value, __ATOMIC_RELAXED);
}

inline void atomic_store(int32_t& v, int32_t value)
{
    __atomic_store_n(&v, value, __ATOMIC_RELAXED);
}

template<typename type>
requires(is_decimal<type>)
inline void atomic_store(type& v, type value)
{
    atomic_store(v.value, value.value);
}

inline uint64_t atomic_load(uint64_t& v)
{
    return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

inline int64_t atomic_load(int64_t& v)
{
    return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

inline uint32_t atomic_load(uint32_t& v)
{
    return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

inline uint32_t atomic_load(int32_t& v)
{
    return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

template<typename type>
type* atomic_load(type* ptr)
{
    return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_load(type& v)
{
    return {atomic_load(v.value)};
}

inline bool atomic_compare_exchange(uint64_t& v, uint64_t from, uint64_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(int64_t& v, int64_t from, int64_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(uint32_t& v, uint32_t from, uint32_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(int32_t& v, int32_t from, int32_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

template<typename type>
requires(is_decimal<type>)
inline bool atomic_compare_exchange(type& v, type from, type to)
{
    return atomic_compare_exchange(v.value, from.value, to.value);
}

template<typename type>
bool atomic_compare_exchange(type*& v, type* from, type* to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(const char*& v, const char* from, const char* to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}


template<typename type>
type atomic_exchange(type* ptr, type val)
{
    return __atomic_exchange_n(ptr, val, __ATOMIC_RELAXED);
}

