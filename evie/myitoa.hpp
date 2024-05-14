/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "str_holder.hpp"
#include "type_traits.hpp"
#include "limits.hpp"

#include <new>

struct str_exception : std::exception
{
    const char* msg;

    str_exception(const char* msg) : msg(msg)
    {
    }
    const char* what() const noexcept
    {
        return msg;
    }
};

namespace my_cvt
{
    uint32_t itoa(char *buf, int8_t v);
    uint32_t itoa(char *buf, uint8_t v);
    uint32_t itoa(char *buf, uint16_t v);
    uint32_t itoa(char *buf, int16_t v);
    uint32_t itoa(char *buf, uint32_t v);
    uint32_t itoa(char *buf, int32_t v);
    uint32_t itoa(char *buf, uint64_t v);
    uint32_t itoa(char *buf, int64_t v);
    uint32_t dtoa(char *buf, double v);

    inline uint32_t itoa(char* buf, bool t) {
        buf[0] = (t ? '1' : '0');
        return 1;
    }

    struct exception : std::exception
    {
        char buf[64];

        exception(str_holder h, str_holder m);
        const char* what() const noexcept;
    };

    template<typename type>
    type atoi_u(const char* buf, uint32_t size) {
        static_assert(is_unsigned_v<type>);
        type ret = 0;
        static const type mm = limits<type>::max / 10;
        for(uint32_t i = 0; i != size; ++i) {
            if(ret > mm) [[unlikely]]
                throw exception(str_holder("atoi() max possible size exceed for: "), {buf, size});
            char ch = buf[i];
            if(ch < '0' || ch > '9') [[unlikely]]
                throw exception(str_holder("atoi() bad integral number: "), {buf, size});
            ret *= 10;
            ret += (ch - '0');
        }
        return ret;
    }

    template<typename type>
    type atoi(const char* buf, uint32_t size) {
        static_assert(is_integral_v<type>);
        typedef make_unsigned_t<type> type_u;
        if constexpr(is_unsigned_v<type>)
            return atoi_u<type_u>(buf, size);
        else {
            if(size && buf[0] == '-')
                return -type(atoi_u<type_u>(buf + 1, size - 1));
            return type(atoi<type_u>(buf, size));
        }
    }

    template<>
    inline bool atoi<bool>(const char* buf, uint32_t size) {
        if(size != 1 || (buf[0] != '0' && buf[0] != '1')) [[unlikely]]
            throw exception(str_holder("atoi() bad bool number: "), {buf, size});
        return(buf[0] == '1');
    }

    template<uint32_t v>
    struct pow10
    {
        static constexpr uint64_t result = pow10<v - 1>::result * 10;
    };

    template<> struct pow10<0>
    {
        static constexpr uint64_t result = 1;
    };

    template<uint32_t v>
    constexpr uint64_t p10()
    {
        return pow10<v>::result;
    }
    
    static constexpr uint64_t decimal_pow[] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
        p10<9>(), p10<10>(), p10<11>(), p10<12>(), p10<13>(),
        p10<14>(), p10<15>(), p10<16>(), p10<17>(), p10<18>(),
        p10<19>()
    };

    static constexpr uint32_t atoi_u_ps[] = {3, 5, 10, 20, 20};
    static constexpr uint32_t atoi_s_ps[] = {4, 6, 11, 21, 21};

    template<typename type>
    struct atoi_size
    {
        static const uint32_t value = (is_unsigned_v<type> ? atoi_u_ps : atoi_s_ps)[sizeof(type) / 2];
    };
}

template<typename type>
requires(is_integral_v<type>)
type lexical_cast(char_cit from, char_cit to)
{
    if(from == to)
        throw str_exception("lexical_cast<integral>() from == to");
    return my_cvt::atoi<type>(from, to - from);
}

template<typename type>
requires(!(is_integral_v<type>))
type lexical_cast(char_cit from, char_cit to);

template<> double lexical_cast<double>(char_cit from, char_cit to);

template<typename type>
type lexical_cast(const str_holder& str)
{
    return lexical_cast<type>(str.begin(), str.end());
}

template<typename ... args>
inline void my_unused(args& ...) {
}

#define MY_UNUSED(name)

