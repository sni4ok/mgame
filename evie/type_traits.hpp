/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef TYPE_TRAITS_HPP
#define TYPE_TRAITS_HPP

#include <cstdint>

template<typename t>
struct is_signed
{
    static constexpr bool value = t(t() - t(1)) < t();
};

template<typename t>
struct is_unsigned
{
    static constexpr bool value = !is_signed<t>::value;
};

struct false_type
{
    static constexpr bool value = false;
};

struct true_type
{
    static constexpr bool value = true;
};

template<typename type>
struct is_integral : false_type {};

#define SET_INTEGRAL(t) \
template<> \
struct is_integral<t> \
{ \
    static constexpr bool value = true; \
};

#define SET_INTEGRALE(st, ut) \
    SET_INTEGRAL(st) \
    SET_INTEGRAL(ut)

template<typename t>
struct make_unsigned
{
};

template<typename t>
struct make_signed
{
};

#define SET_UNSIGNED(t, st, ut) \
template<> \
struct make_unsigned<t> \
{ \
    typedef ut type; \
}; \
template<> \
struct make_signed<t> \
{ \
    typedef st type; \
};

#define SET_INTEGRAL_TYPE(st, ut) \
    SET_INTEGRALE(st, ut) \
    SET_UNSIGNED(st, st, ut) \
    SET_UNSIGNED(ut, st, ut)

#define SET_INTEGRAL_TYPE_E(t) \
    SET_INTEGRAL_TYPE(signed t, unsigned t)

#define PP ()
#define EXPAND(...) EXPAND4(__VA_ARGS__)
#define EXPAND4(...) EXPAND3(__VA_ARGS__)
#define EXPAND3(...) EXPAND2(__VA_ARGS__)
#define EXPAND2(...) EXPAND1(__VA_ARGS__)
#define EXPAND1(...) __VA_ARGS__
#define FOR_EACH(macro, ...) \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) \
    macro(a1) \
  __VA_OPT__(FOR_EACH_AGAIN PP (macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

__extension__ typedef __int128 int128_t;
__extension__ typedef  __uint128_t uint128_t;

FOR_EACH(SET_INTEGRAL_TYPE_E, char, short, int, long, long long)
SET_INTEGRAL_TYPE(int128_t, uint128_t)
FOR_EACH(SET_INTEGRAL, char, wchar_t, bool)

template<typename t>
struct remove_reference
{
    using type = t;
};

template<typename t>
struct remove_reference<t&>
{
    using type = t;
};

template<typename t>
struct remove_reference<t&&>
{
    using type = t;
};

template<typename t>
struct is_trivially_copyable
{
    static constexpr bool value = __is_trivially_copyable(t);
};

template<typename t>
struct is_trivially_destructible 
{
#ifdef CLANG_COMPILER
    static constexpr bool value = __is_trivially_destructible(t);
#else
    static constexpr bool value = __has_trivial_destructor(t);
#endif
};

template<typename t>
struct is_polymorphic
{
    static constexpr bool value = __is_polymorphic(t);
};

template<typename t>
struct remove_cv
{
    using type = __remove_cv(t);
};

template<typename t>
using __remove_cv_t = typename remove_cv<t>::type;

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

template<typename t>
struct remove_pointer
: public __remove_pointer_helper<t, __remove_cv_t<t> >
{
};

template<typename t, typename p>
struct is_same
{
    static constexpr bool value = __is_same(t, p);
};

template<typename type>
inline constexpr bool is_unsigned_v = is_unsigned<type>::value;

template<typename type>
inline constexpr bool is_signed_v = is_signed<type>::value;

template<typename type>
inline constexpr bool is_integral_v = is_integral<type>::value;

template<typename type>
using make_unsigned_t = typename make_unsigned<type>::type;

template<typename type>
using make_signed_t = typename make_signed<type>::type;

template<typename type>
inline constexpr bool is_array_v = false;
template<typename type>
inline constexpr bool is_array_v<type[]> = true;
template<typename type, uint64_t count>
inline constexpr bool is_array_v<type[count]> = true;

template<typename type>
using remove_reference_t = remove_reference<type>::type;

template<typename type>
inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<type>::value;

template<typename type>
inline constexpr bool is_trivially_destructible_v = is_trivially_destructible<type>::value;

template<typename type>
inline constexpr bool is_polymorphic_v = is_polymorphic<type>::value;

template<typename type>
using remove_pointer_t = typename remove_pointer<type>::type;

template<typename t, typename p>
inline constexpr bool is_same_v = is_same<t, p>::value;

template<typename type>
struct is_numeric
{
    static constexpr bool value = is_integral_v<type> || is_same_v<type, double>;
};

template<typename type>
inline constexpr bool is_numeric_v = is_numeric<type>::value;

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

template<bool cond, typename t, typename p>
using conditional_t = typename conditional<cond, t, p>::type;

template<typename>
struct is_function : false_type {};

template<typename ret, typename... args>
struct is_function<ret(args...)> : true_type {};

template<typename ret, typename... args>
struct is_function<ret(args...) const> : true_type {};

template<typename t>
struct is_member_function_pointer_helper : false_type {};

template<typename t, typename p>
struct is_member_function_pointer_helper<t p::*> : is_function<t>
{
};

template<typename t>
struct is_member_function_pointer : is_member_function_pointer_helper<typename remove_cv<t>::type>
{
};

template<typename type>
inline constexpr bool is_member_function_pointer_v = is_member_function_pointer<type>::value;

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
inline constexpr bool is_pointer_v = is_pointer<type>::value;

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
using add_const_t = add_const<type>::type;

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
using remove_const_t = remove_const<type>::type;

template <typename type>
inline constexpr bool is_class_v = __is_class(type);

template<typename type>
[[nodiscard]] constexpr remove_reference_t<type>&& move(type&& t) noexcept
{
    return static_cast<remove_reference_t<type>&&>(t);
}

template<typename type>
void simple_swap(type& a, type& b)
{
    type tmp = a;
    a = b;
    b = tmp;
}

template<typename t>
concept __have_tuple_size = is_class_v<t> && requires(t* v)
{
    v->tuple_size();
};

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

#endif

