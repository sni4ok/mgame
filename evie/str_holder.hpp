/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "assert.hpp"

extern "C"
{
#define NE noexcept
#define NN(...) __attribute__ ((__nonnull__  (__VA_ARGS__) ))
#define PURE __attribute__ ((__pure__))
#define UR __attribute__ ((__warn_unused_result__))
#define AM __attribute__ ((__malloc__))
#define AS(...) __attribute__ ((__alloc_size__  (__VA_ARGS__) ))

    extern void* memset(void*, int, size_t) NE NN(1);
    extern void* memcpy(void*, const void*, size_t) NE NN(1, 2);
    extern void* memmove(void*, const void*, size_t) NE NN(1, 2);
    extern int memcmp(const void*, const void*, size_t) NE PURE NN(1, 2);
    extern int strcmp(char_cit, char_cit) NE PURE NN(1, 2);
    extern void* malloc (size_t) NE AM AS(1) UR;
    extern void* realloc(void*, size_t) NE UR AS(2);
    extern void free(void*) NE;
    extern char_it getenv(char_cit) NE NN(1) UR;
    extern int usleep(u32);
    extern int close(int);
    extern int system(char_cit);
    extern char_it strdup(char_cit) NE AM NN(1);

#undef NE
#undef NN
#undef PURE
#undef UR
#undef AM
#undef AS
}

char_cit find(char_cit from, char_cit to, char c);
char_it find(char_it from, char_it to, char c);

static const char endl = '\n';

template<typename type>
class span
{
    const type* data;
    u64 size_;

public:
    typedef const type* iterator;

    constexpr span() : data(), size_()
    {
    }
    constexpr span(iterator data, u64 size) : data(data), size_(size)
    {
    }
    constexpr span(iterator from, iterator to) : data(from), size_(to - from)
    {
    }

    template<u64 sz>
    constexpr span(const type (&buf)[sz]) : data(buf), size_(sz - 1)
    {
    }
    constexpr iterator begin() const
    {
        return data;
    }
    constexpr iterator end() const
    {
        return data + size_;
    }
    constexpr auto& operator[](u64 idx) const
    {
        return *(data + idx);
    }
    constexpr auto& back() const
    {
        ASSERT(size_);
        return *(end() - 1);
    }
    constexpr u64 size() const
    {
        return size_;
    }
    constexpr bool empty() const
    {
        return !size_;
    }
    constexpr void resize(u64 size)
    {
        ASSERT(size <= size_);
        size_ = size;
    }
    constexpr void pop_back()
    {
        ASSERT(size_);
        --size_;
    }
};

typedef span<char> str_holder;

str_holder _str_holder(char_cit str);

template<u64 sz>
constexpr str_holder from_array(const char (&str)[sz])
{
    u64 size = sz;
    while(size && str[size - 1] == char())
        --size;
    return {str, str + size};
}

template<typename stream>
stream& operator<<(stream& s, str_holder str)
{
    s.write(str.begin(), str.size());
    return s;
}

bool operator==(str_holder l, str_holder r);

[[noreturn]] void throw_exception(str_holder);
[[noreturn]] void throw_exception(str_holder, str_holder);

