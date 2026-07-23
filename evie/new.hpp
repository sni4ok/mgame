/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef _NEW
#define _NEW

#include "stdint.hpp"

inline void* operator new(size_t, void* p)
{
    return p;
}   

namespace std
{
    struct nothrow_t
    {
    };

    enum class align_val_t: size_t
    {
    };

    static const nothrow_t nothrow;
}

[[__nodiscard__]] void* operator new(size_t, std::align_val_t, const std::nothrow_t&)
    noexcept  __attribute__((__alloc_size__ (1), __alloc_align__ (2), __malloc__));

void* operator new(size_t, const std::nothrow_t&)
    noexcept  __attribute__((__alloc_size__ (1), __malloc__));

#endif

