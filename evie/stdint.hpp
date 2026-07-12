#pragma once

typedef signed char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;

#ifdef __x86_64
    typedef long int i64;
    typedef unsigned long u64;
    typedef u64 size_t;
#else
    typedef long long i64;
    typedef unsigned long long u64;
    typedef u32 size_t;
#endif

static_assert(sizeof(i16) == 2);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(i64) == 8);
static_assert(sizeof(u64) == 8);

typedef char* char_it;
typedef const char* char_cit;

struct forward_iterator_tag;
struct bidirectional_iterator_tag;

template<typename type>
void simple_swap(type& a, type& b)
{
    type tmp = a;
    a = b;
    b = tmp;
}

template<bool const, typename t, typename p>
struct conditional
{
    typedef t type;
};

template<typename t, typename p>
struct conditional<false, t, p>
{
    typedef p type;
};

