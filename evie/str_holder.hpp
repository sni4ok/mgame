/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

#include <cassert>

extern "C"
{
    extern void* memset(void*, int, size_t) __THROW __nonnull ((1));
    extern void* memcpy(void*, const void*, size_t) __THROW __nonnull ((1, 2));
    extern void* memmove(void*, const void*, size_t) __THROW __nonnull ((1, 2));
    extern int memcmp(const void*, const void*, size_t) __THROW __attribute_pure__ __nonnull ((1, 2));

    extern void *malloc (size_t __size) __THROW __attribute_malloc__ __attribute_alloc_size__ ((1)) __wur;
    extern void *realloc(void*, size_t) __THROW __attribute_warn_unused_result__ __attribute_alloc_size__ ((2));
    extern void free(void*) __THROW;
    extern char *getenv(const char*) __THROW __nonnull ((1)) __wur;
    extern int usleep(__useconds_t __useconds);
    extern int close(int __fd);
    extern int system (const char *__command);
}

static const char endl = '\n';

typedef char* char_it;
typedef const char* char_cit;

class str_holder
{
    char_cit str;
    uint64_t size_;

public:
    constexpr str_holder() : str(), size_()
    {
    }
    constexpr str_holder(char_cit str, uint64_t size) : str(str), size_(size)
    {
    }
    constexpr str_holder(char_cit from, char_cit to) : str(from), size_(to - from)
    {
    }

    template<uint64_t sz>
    constexpr str_holder(const char (&str)[sz]) : str(str), size_(sz - 1)
    {
    }
    constexpr char_cit begin() const
    {
        return str;
    }
    constexpr char_cit end() const
    {
        return str + size_;
    }
    constexpr char operator[](uint32_t idx) const
    {
        return *(str + idx);
    }
    constexpr char back() const
    {
        assert(size_);
        return *(end() - 1);
    }
    constexpr uint64_t size() const
    {
        return size_;
    }
    constexpr bool empty() const {
        return !size_;
    }
    constexpr void resize(uint64_t size) {
        assert(size <= size_);
        size_ = size;
    }
    constexpr void pop_back() {
        assert(size_);
        --size_;
    }
};

str_holder _str_holder(char_cit str);

template<uint64_t sz>
constexpr str_holder from_array(const char (&str)[sz])
{
    uint64_t size = sz;
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

