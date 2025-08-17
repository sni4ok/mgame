/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "decimal.hpp"

#include <cstdint>

inline uint64_t atomic_add(uint64_t& v, uint64_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_add_fetch(&v, value, m);
}

inline int64_t atomic_add(int64_t& v, int64_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_add_fetch(&v, value, m);
}

inline uint32_t atomic_add(uint32_t& v, uint32_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_add_fetch(&v, value, m);
}

inline int32_t atomic_add(int32_t& v, int32_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_add_fetch(&v, value, m);
}

inline uint16_t atomic_add(uint16_t& v, uint16_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_add_fetch(&v, value, m);
}

inline int16_t atomic_add(int16_t& v, int16_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_add_fetch(&v, value, m);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_add(type& v, type value, int m = __ATOMIC_RELAXED)
{
    return {atomic_add(v.value, value.value, m)};
}

inline uint64_t atomic_sub(uint64_t& v, uint64_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_sub_fetch(&v, value, m);
}

inline int64_t atomic_sub(int64_t& v, int64_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_sub_fetch(&v, value, m);
}

inline uint32_t atomic_sub(uint32_t& v, uint32_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_sub_fetch(&v, value, m);
}

inline int32_t atomic_sub(int32_t& v, int32_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_sub_fetch(&v, value, m);
}

inline uint16_t atomic_sub(uint16_t& v, uint16_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_sub_fetch(&v, value, m);
}

inline int16_t atomic_sub(int16_t& v, int16_t value, int m = __ATOMIC_RELAXED)
{
    return __atomic_sub_fetch(&v, value, m);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_sub(type& v, type value, int m = __ATOMIC_RELAXED)
{
    return {atomic_sub(v.value, value.value, m)};
}

inline void atomic_store(uint64_t& v, uint64_t value, int m = __ATOMIC_RELAXED)
{
    __atomic_store_n(&v, value, m);
}

inline void atomic_store(int64_t& v, int64_t value, int m = __ATOMIC_RELAXED)
{
    __atomic_store_n(&v, value, m);
}

inline void atomic_store(uint32_t& v, uint32_t value, int m = __ATOMIC_RELAXED)
{
    __atomic_store_n(&v, value, m);
}

inline void atomic_store(int32_t& v, int32_t value, int m = __ATOMIC_RELAXED)
{
    __atomic_store_n(&v, value, m);
}

template<typename type>
requires(is_decimal<type>)
inline void atomic_store(type& v, type value, int m = __ATOMIC_RELAXED)
{
    atomic_store(v.value, value.value, m);
}

inline uint64_t atomic_load(uint64_t& v, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&v, m);
}

inline int64_t atomic_load(int64_t& v, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&v, m);
}

inline uint32_t atomic_load(uint32_t& v, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&v, m);
}

inline uint32_t atomic_load(int32_t& v, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&v, m);
}

inline uint32_t atomic_load(uint16_t& v, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&v, m);
}

inline uint32_t atomic_load(int16_t& v, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&v, m);
}

template<typename type>
type* atomic_load(type* ptr, int m = __ATOMIC_RELAXED)
{
    return __atomic_load_n(&ptr, m);
}

template<typename type>
requires(is_decimal<type>)
inline type atomic_load(type& v, int m = __ATOMIC_RELAXED)
{
    return {atomic_load(v.value, m)};
}

inline bool atomic_compare_exchange(uint64_t& v, uint64_t from, uint64_t to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

inline bool atomic_compare_exchange(int64_t& v, int64_t from, int64_t to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

inline bool atomic_compare_exchange(uint32_t& v, uint32_t from, uint32_t to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

inline bool atomic_compare_exchange(int32_t& v, int32_t from, int32_t to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

inline bool atomic_compare_exchange(uint16_t& v, uint16_t from, uint16_t to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

inline bool atomic_compare_exchange(int16_t& v, int16_t from, int16_t to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

template<typename type>
requires(is_decimal<type>)
inline bool atomic_compare_exchange(type& v, type from, type to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return atomic_compare_exchange(v.value, from.value, to.value, sm, fm);
}

template<typename type>
bool atomic_compare_exchange(type*& v, type* from, type* to, int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

inline bool atomic_compare_exchange(const char*& v, const char* from, const char* to,
    int sm = __ATOMIC_RELAXED, int fm = __ATOMIC_RELAXED)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, sm, fm);
}

template<typename type>
type atomic_exchange(type* ptr, type val, int m = __ATOMIC_RELAXED)
{
    return __atomic_exchange_n(ptr, val, m);
}

