/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "str_holder.hpp"
#include "type_traits.hpp"
#include "decimal.hpp"
#include "limits.hpp"

#include <new>

typedef std::exception exception;

struct str_exception : exception
{
    char_cit msg;

    str_exception(char_cit msg) : msg(msg)
    {
    }
    char_cit what() const noexcept
    {
        return msg;
    }
};

template<typename stream>
stream& operator<<(stream& s, const exception& e)
{
    s << "exception: " << _str_holder(e.what());
    return s;
}

namespace cvt
{
    u32 itoa(char_it buf, bool t);
    u32 itoa(char_it buf, char v);
    u32 itoa(char_it buf, i8 v);
    u32 itoa(char_it buf, u8 v);
    u32 itoa(char_it buf, u16 v);
    u32 itoa(char_it buf, i16 v);
    u32 itoa(char_it buf, u32 v);
    u32 itoa(char_it buf, i32 v);
    u32 itoa(char_it buf, u64 v);
    u32 itoa(char_it buf, i64 v);
    u32 itoa(char_it buf, double v);
    u32 log10(u32 i);

#ifdef USE_INT128_EXT
    u32 itoa(char_it buf, u128 v);
    u32 itoa(char_it buf, i128 v);
#endif

    struct exception : ::exception
    {
        char buf[64];

        exception(str_holder h, str_holder m);
        char_cit what() const noexcept;
    };

    template<typename type>
    type atoi_u(char_cit buf, u32 size)
    {
        static_assert(is_unsigned_v<type>);
        type ret = 0;
        static const type mm = limits<type>::max / 10;

        for(u32 i = 0; i != size; ++i)
        {
            if(ret > mm) [[unlikely]]
                throw exception("atoi() max possible size exceed for: ", {buf, size});

            char ch = buf[i];
            if(ch < '0' || ch > '9') [[unlikely]]
                throw exception("atoi() bad integral number: ", {buf, size});

            ret *= 10;
            ret += (ch - '0');
        }
        return ret;
    }

    template<typename type>
    type atoi(char_cit buf, u32 size)
    {
        static_assert(is_integral_v<type>);
        typedef make_unsigned_t<type> type_u;
        if constexpr(is_unsigned_v<type>)
            return atoi_u<type_u>(buf, size);
        else
        {
            if(size && buf[0] == '-')
                return -type(atoi_u<type_u>(buf + 1, size - 1));
            return type(atoi<type_u>(buf, size));
        }
    }

    template<>
    inline char atoi<char>(char_cit buf, u32 size)
    {
        if(size != 1) [[unlikely]]
            throw exception("atoi() bad char size: ", {buf, size});
        return *buf;
    }

    template<>
    inline bool atoi<bool>(char_cit buf, u32 size)
    {
        if(size != 1 || (buf[0] != '0' && buf[0] != '1')) [[unlikely]]
            throw exception("atoi() bad bool number: ", {buf, size});
        return(buf[0] == '1');
    }

    template<u32 v>
    struct pow10
    {
        static constexpr u64 result = pow10<v - 1>::result * 10;
    };

    template<>
    struct pow10<0>
    {
        static constexpr u64 result = 1;
    };

    template<u32 v>
    constexpr u64 p10()
    {
        return pow10<v>::result;
    }
    
    static constexpr u64 pow[] = 
    {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
        p10<9>(), p10<10>(), p10<11>(), p10<12>(), p10<13>(),
        p10<14>(), p10<15>(), p10<16>(), p10<17>(), p10<18>(),
        p10<19>()
    };

    static constexpr u32 atoi_u_ps[] = {3, 5, 10, 0, 20, 0, 0, 0, 39};
    static constexpr u32 atoi_s_ps[] = {4, 6, 11, 0, 21, 0, 0, 0, 40};

    template<typename type>
    struct atoi_size
    {
        static const u32 value = (is_unsigned_v<type> ? atoi_u_ps : atoi_s_ps)
            [sizeof(type) / 2];
    };

    template<>
    struct atoi_size<double>
    {
        static const u32 value = 30;
    };

    template<>
    struct atoi_size<char>
    {
        static const u32 value = 1;
    };

    template<>
    struct atoi_size<bool>
    {
        static const u32 value = 1;
    };

    template<typename type>
    inline constexpr u32 atoi_size_v = atoi_size<type>::value;
}

i64 read_decimal_impl(char_cit it, char_cit ie, int exponent);

template<typename decimal>
inline decimal lexical_cast(char_cit it, char_cit ie)
    requires(is_decimal<decimal> && decimal::exponent != 0)
{
    decimal ret;
    ret.value = read_decimal_impl(it, ie, decimal::exponent);
    return ret;
}

template<typename type>
requires(is_integral_v<type>)
type lexical_cast(char_cit from, char_cit to)
{
    if(from == to)
        throw str_exception("lexical_cast<integral>() from == to");
    return cvt::atoi<type>(from, to - from);
}

struct mstring;
template<typename type>
requires(!(is_integral_v<type> || is_same_v<type, mstring> || is_decimal<type>))
type lexical_cast(char_cit from, char_cit to);

template<> double lexical_cast<double>(char_cit from, char_cit to);

template<typename type>
type lexical_cast(str_holder str)
{
    return lexical_cast<type>(str.begin(), str.end());
}

template<>
inline str_holder lexical_cast<str_holder>(char_cit from, char_cit to)
{
    return {from, to};
}

template<typename ... args>
inline void unused(args& ...)
{
}

struct ios_base
{
};

template<typename type>
concept __derived_from_ios_base = is_class_v<type> && (!is_same_v<type, ios_base>)
&& requires(type* t, ios_base* b)
{
    b = t;
};

template<typename stream, typename type>
requires __derived_from_ios_base<stream> && requires(stream& s, const type& t)
{
    s << t;
}

using __rvalue_stream_insertion_t = stream&&;

template<typename stream, typename type>
inline __rvalue_stream_insertion_t<stream, type> operator<<(stream&& s, const type& t)
{
  s << t;
  return move(s);
}

template<typename stream, typename type>
requires(__derived_from_ios_base<stream> && is_numeric_v<type>)
stream& operator<<(stream& s, type v)
{
    s.write_numeric(v);
    return s;
}

template<typename stream, typename array>
requires(__derived_from_ios_base<stream> && is_array_v<array>)
stream& operator<<(stream& s, const array& v)
{
    s << from_array(v);
    return s;
}

