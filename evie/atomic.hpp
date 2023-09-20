/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

inline void atomic_add(volatile uint64_t& v, uint64_t value)
{
    __atomic_add_fetch(&v, value, __ATOMIC_RELAXED);
}

inline uint64_t atomic_load(uint64_t& v)
{
    return __atomic_load_n(&v, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(uint64_t& v, uint64_t from, uint64_t to)
{
    return __atomic_compare_exchange_n(&v, &from, to, false/*weak*/, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline bool atomic_compare_exchange(const char*& v, const char* from, const char* to)
{
    return __atomic_compare_exchange_n(&v, &from, to, false/*weak*/, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

