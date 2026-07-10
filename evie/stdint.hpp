#pragma once

typedef signed char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;

#ifdef __x86_64
    typedef unsigned int u32;
    typedef long int i64;
    typedef unsigned long u64;
#else
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
#endif

typedef unsigned long size_t;

static_assert(sizeof(i16) == 2);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(i64) == 8);
static_assert(sizeof(u64) == 8);

typedef char* char_it;
typedef const char* char_cit;

template<typename t>
struct is_signed
{
    static constexpr bool value = t(t() - t{1}) < t();
};

template<typename t>
struct is_unsigned
{
    static constexpr bool value = !is_signed<t>::value;
};

template <typename type>
inline constexpr bool is_class_v = __is_class(type);

