/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <string>
#include <sstream>
#include <limits>
#include <type_traits>

#define unlikely(x)     __builtin_expect(!!(x),0)
#define likely(x)       __builtin_expect(!!(x),1)

class es
{
    std::string s;
    std::stringstream ss;
public:
    template<typename t>
    es& operator%(const t& v) {
        ss << v;
        return *this;
    }
    operator const char*() {
        s = ss.str();
        return s.c_str();
    }
};

namespace my_cvt
{
    uint32_t utoa16(char *aBuf, uint16_t v);
    uint32_t itoa16(char *aBuf, int16_t v);
    uint32_t utoa32(char *aBuf, uint32_t v);
    uint32_t itoa32(char *aBuf, int32_t v);
    uint32_t utoa64(char *aBuf, uint64_t v);
    uint32_t itoa64(char *aBuf, int64_t v);
    uint32_t dtoa(char *buf, double v);

    inline uint32_t itoa(char* buf, bool t) {
        buf[0] = (t ? '1' : '0');
        return 1;
    }
    using std::is_integral;
    using std::is_unsigned;
    using std::is_signed;
    using std::is_same;

    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_unsigned<T>::value && sizeof(T) == 1, uint32_t>
    itoa(char* buf, T t) {
        return utoa16(buf, uint16_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_unsigned<T>::value && sizeof(T) == 2, uint32_t>
    itoa(char* buf, T t) {
        return utoa16(buf, uint16_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_unsigned<T>::value && sizeof(T) == 4, uint32_t>
    itoa(char* buf, T t) {
        return utoa32(buf, uint32_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_unsigned<T>::value && sizeof(T) == 8, uint32_t>
    itoa(char* buf, T t) {
        return utoa64(buf, uint64_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_signed<T>::value && sizeof(T) == 1, uint32_t>
    itoa(char* buf, T t) {
        return itoa16(buf, int16_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_signed<T>::value && sizeof(T) == 2, uint32_t>
    itoa(char* buf, T t) {
        return itoa16(buf, int16_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_signed<T>::value && sizeof(T) == 4, uint32_t>
    itoa(char* buf, T t) {
        return itoa32(buf, int32_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_integral<T>::value && is_signed<T>::value && sizeof(T) == 8, uint32_t>
    itoa(char* buf, T t) {
        return itoa64(buf, int64_t(t));
    }
    template<typename T>
    typename std::enable_if_t<is_same<T, bool>::value, bool>
    atoi(const char* buf, uint32_t size) {
        if(unlikely(size != 1 || (buf[0] != '0' && buf[0] != '1')))
            throw std::runtime_error(es() % "atoi() bad bool number: " % std::string(&buf[0], &buf[size]));
        return(buf[0] == '1');
    }
    template<typename T>
    typename std::enable_if_t<!(is_same<T, bool>::value) && is_integral<T>::value && is_unsigned<T>::value, T>
    atoi(const char* buf, uint32_t size) {
        T ret = 0;
        static const T mm = (std::numeric_limits<T>::max()) / 10;
        for(uint32_t i = 0; i != size; ++i) {
            if(unlikely(ret > mm))
                throw std::runtime_error(es() % "atoi() max possible size exceed for: "
                    % std::string(&buf[0], &buf[size]) % ", ret: " % ret % ", mm: " % mm);
            char ch = buf[i];
            if(unlikely(ch < '0' || ch > '9'))
                throw std::runtime_error(es() % "atoi() bad integral number: " % std::string(&buf[0], &buf[size]));
            ret *= 10;
            ret += (ch - '0');
        }
        return ret;
    }
    template<typename T>
    typename std::enable_if_t<!(is_same<T, bool>::value) && is_integral<T>::value && is_signed<T>::value, T>
    atoi(const char* buf, uint32_t size) {
        if(size && buf[0] == '-')
            return -T(atoi<typename std::make_unsigned<T>::type>(buf + 1, size - 1));
        return T(atoi<typename std::make_unsigned<T>::type>(buf, size));
    }
    template<uint32_t v>
    struct pow10
    {
        static constexpr uint64_t result = pow10<v - 1>::result * 10;
    };

    template<> struct pow10<1>
    {
        static constexpr uint64_t result = 10;
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
}

template<typename ... args>
inline void my_unused(args& ...) {
}

#define MY_UNUSED(name)

