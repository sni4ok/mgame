/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <cstdint>

inline uint64_t atomic_add(uint64_t& v, uint64_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int64_t atomic_add(int64_t& v, int64_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline uint64_t atomic_sub(uint64_t& v, uint64_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int64_t atomic_sub(int64_t& v, int64_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

inline uint32_t atomic_add(uint32_t& v, uint32_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int32_t atomic_add(int32_t& v, int32_t value)
{
    return __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline uint32_t atomic_sub(uint32_t& v, uint32_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

inline int32_t atomic_sub(int32_t& v, int32_t value)
{
    return __atomic_sub_fetch(&v, value, __ATOMIC_RELAXED);
}

inline void atomic_store(uint64_t& v, uint64_t value)
{
    __atomic_store_n(&v, value, __ATOMIC_RELAXED);
}

inline void atomic_store(uint32_t& v, uint32_t value)
{
    __atomic_store_n(&v, value, __ATOMIC_RELAXED);
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

inline bool atomic_compare_exchange(uint32_t& v, uint32_t from, uint32_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(uint16_t& v, uint16_t from, uint16_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(uint64_t& v, uint64_t from, uint64_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(bool& v, bool from, bool to)
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

template<typename type>
type* atomic_load(type* ptr)
{
    return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename type>
bool atomic_compare_exchange(type*& v, type* from, type* to)
{
    return __atomic_compare_exchange_n(&v, &from, to, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline uint16_t atomic_exchange(uint16_t& v, uint16_t to)
{
    return __atomic_exchange_n(&v, to, __ATOMIC_RELAXED);
}

