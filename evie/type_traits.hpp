/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "iterator.hpp"

struct false_type
{
    static const bool value = false;
};

struct true_type
{
    static const bool value = true;
};

struct i128d
{
    double value;

    explicit i128d(double v) : value(v)
    {
    }
    i128d() : value()
    {
    }
    template<typename type>
    i128d operator*(type v) const
    {
        return i128d(value * v);
    }
    template<typename type>
    i128d operator/(type v) const
    {
        return i128d(value / v);
    }
    i128d& operator+=(i128d r)
    {
        value += r.value;
        return *this;
    }
    bool operator<(i128d r) const
    {
        return value < r.value;
    }
    operator double() const
    {
        return value;
    }
};

#ifdef __x86_64
    #define USE_INT128_EXT
#endif

#ifdef USE_INT128_EXT
__extension__ typedef __int128 i128;
__extension__ typedef __uint128_t u128;

#else
    typedef i128d i128;
    typedef i128d u128;
#endif

template<typename stream>
stream& operator<<(stream& s, i128d v);

template<typename t>
struct is_trivially_copyable
{
    static const bool value = __is_trivially_copyable(t);
};

template<typename t>
struct is_polymorphic
{
    static const bool value = __is_polymorphic(t);
};

template<typename t, typename>
struct __remove_pointer_helper
{
    typedef t type;
};

template<typename t, typename p>
struct __remove_pointer_helper<t, p*>
{
    typedef p type;
};

template<typename type>
using remove_const_t = typename remove_const<type>::type;

template<typename t>
struct remove_pointer : public __remove_pointer_helper<t, remove_const_t<t> >
{
};

template<typename t, typename p>
struct is_same
{
    static const bool value = __is_same(t, p);
};

template<typename type>
static const bool is_array_v = false;

template<typename type>
static const bool is_array_v<type[]> = true;

template<typename type, u64 count>
static const bool is_array_v<type[count]> = true;

template<typename type>
using remove_reference_t = typename remove_reference<type>::type;

template<typename type>
static const bool is_trivially_copyable_v = is_trivially_copyable<type>::value;

template<typename type>
static const bool is_trivially_destructible_v = is_trivially_destructible<type>::value;

template<typename type>
static const bool is_polymorphic_v = is_polymorphic<type>::value;

template<typename type>
using remove_pointer_t = typename remove_pointer<type>::type;

template<typename t, typename p>
static const bool is_same_v = is_same<t, p>::value;

template <typename type>
inline const bool is_class_v = __is_class(type);

template<typename type>
concept __is_integral_impl = !is_class_v<type> &&
    is_same_v<type, remove_reference_t<type> > && requires(remove_const_t<type> a, type b)
{
    a = a << b - a;
};

template<typename type>
struct is_integral
{
    static const bool value = __is_integral_impl<type>;
};

template<typename type>
static const bool is_integral_v = __is_integral_impl<type>;

template<typename t>
concept __make_signed_impl =
    is_same_v<t, remove_reference_t<t> > && requires(remove_const_t<t> a)
{
    a = t() - t{1};
    a < t();
};

template<typename t>
struct is_signed
{
    static constexpr bool get()
    {
        if constexpr(__make_signed_impl<t>)
            return t(t() - t{1}) < t();
        else
            return false;
    }
    static const bool value = get();
};

template<typename t>
struct is_unsigned
{
    static const bool value = !is_signed<t>::value;
};

template<typename type>
static const bool is_unsigned_v = is_unsigned<type>::value;

template<typename type>
static const bool is_signed_v = is_signed<type>::value;

template<typename type>
struct is_numeric
{
    static const bool value = is_integral_v<type> || is_same_v<type, double>;
};

template<typename type>
static const bool is_numeric_v = is_numeric<type>::value;

template<bool cond, typename t, typename p>
using conditional_t = typename conditional<cond, t, p>::type;

template<typename>
struct is_function : false_type
{
};

template<typename ret, typename... args>
struct is_function<ret(args...)> : true_type
{
};

template<typename ret, typename... args>
struct is_function<ret(args...) const> : true_type
{
};

template<typename t>
struct is_member_function_pointer_helper : false_type
{
};

template<typename t, typename p>
struct is_member_function_pointer_helper<t p::*> : is_function<t>
{
};

template<typename t>
struct is_member_function_pointer
    : is_member_function_pointer_helper<remove_const_t<t> >
{
};

template<typename type>
static const bool is_member_function_pointer_v
    = is_member_function_pointer<type>::value;

template<typename>
struct is_pointer : false_type
{
};

template<typename t>
struct is_pointer<t*> : true_type
{
};

template<typename t>
struct is_pointer<const t*> : true_type
{
};

template<typename type>
static const bool is_pointer_v = is_pointer<type>::value;

template<typename>
struct is_const : false_type
{
};

template<typename t>
struct is_const<const t> : true_type
{
};

template<typename t>
struct add_const
{
    typedef const t type;
};

template<typename t>
struct add_const<const t>
{
    typedef const t type;
};

template<typename type>
using add_const_t = typename add_const<type>::type;

template<typename t1, typename t2 = t1>
concept __have_equal_op = is_class_v<t1> && requires(t1& v1, t2& v2)
{
    v1.operator==(v2);
};

template<typename t1, typename t2 = t1>
concept __have_less_op = is_class_v<t1> && requires(t1& v1, t2& v2)
{
    v1.operator<(v2);
};

template<typename t>
concept __have_begin = is_class_v<t> && requires(t* v)
{
    v->begin();
};

template<typename t>
concept __have_back = is_class_v<t> && requires(t* v)
{
    v->back();
};

template<typename cont>
[[nodiscard]] constexpr auto begin(const cont& c)
{
    if constexpr(is_array_v<cont>)
        return &c[0];
    else
        return c.begin();
}

template<typename cont>
[[nodiscard]] constexpr auto end(const cont& c)
{
    if constexpr(is_array_v<cont>)
        return &c[(sizeof(c) / sizeof(c[0])) - 1];
    else
        return c.end();
}

template<int index, int index_from, typename ... params>
struct conditional_multi_f;

template<int index, int index_from, typename param>
struct conditional_multi_f<index, index_from, param>
{
    typedef conditional_t<index == index_from, param, void> type;
};

template<int index, int index_from, typename param, typename ... params>
struct conditional_multi_f<index, index_from, param, params...>
{
    typedef conditional_t<index == index_from, param,
        typename conditional_multi_f<index, index_from + 1, params...>::type> type;
};

template<int index, typename param, typename ... params>
struct conditional_multi : conditional_multi_f<index, 0, param, params...>
{
};

template<int index, typename param, typename ... params>
using conditional_multi_t = typename conditional_multi<index, param, params...>::type;

template<typename t>
requires(__is_integral_impl<t> && !is_same_v<t, bool>)
struct make_unsigned
{
    typedef conditional_multi_t<sizeof(t) / 2, u8, u16, u32, void, u64
#ifdef USE_INT128_EXT
        , void, void, void, u128
#endif
        > type;
};

template<typename t>
requires(__is_integral_impl<t> && !is_same_v<t, bool>)
struct make_signed
{
    typedef conditional_multi_t<sizeof(t) / 2, i8, i16, i32, void, i64
#ifdef USE_INT128_EXT
        , void, void, void, i128
#endif
        > type;
};

template<typename type>
using make_unsigned_t = typename make_unsigned<type>::type;

template<typename type>
using make_signed_t = typename make_signed<type>::type;


template<typename type>
requires(is_signed<type>::value)
constexpr type abs(type v)
{
    if(v < type())
        return -v;
    return v;
}

