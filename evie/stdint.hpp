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

template <typename type>
inline const bool is_class_v = __is_class(type);

template<typename t>
struct is_trivially_destructible
{
#ifdef CLANG_COMPILER
    static const bool value = __is_trivially_destructible(t);
#else
    static const bool value = __has_trivial_destructor(t);
#endif
};

template<typename t>
struct remove_const
{
    typedef t type;
};

template<typename t>
struct remove_const<const t>
{
    typedef t type;
};

template<typename type>
void simple_swap(type& a, type& b)
{
    type tmp = a;
    a = b;
    b = tmp;
}

