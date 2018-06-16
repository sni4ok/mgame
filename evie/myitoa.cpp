/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "myitoa.hpp"

#include <cmath>
#include <cassert>
#include <stdio.h>
#include <cstring>

namespace my_cvt
{
    template<uint32_t v>
    struct pow10
    {
        static const uint64_t result = pow10<v - 1>::result * 10;
    };

    template<> struct pow10<1>
    {
        static const uint64_t result = 10;
    };

    template<uint32_t digits_s>
    struct itoa_prealloc
    {
        static const uint32_t digits = digits_s;
        static const uint32_t values = pow10<digits>::result;
        char data[values * digits + 1];

        itoa_prealloc()
        {
            char tmp[digits];
            memset(tmp, '0', digits);
            char* ptr = data;
            for(uint32_t i = 0; i != values; ++i)
            {
                memcpy(ptr, tmp, digits);
                ptr += digits;
                if(i == values - 1)
                    break;
                uint32_t cur_idx = digits - 1;
                for(;;)
                {
                    if(tmp[cur_idx] < '9') {
                        ++tmp[cur_idx];
                        break;
                    } else {
                        tmp[cur_idx] = '0';
                        --cur_idx;
                    }
                }
            }
        }

        itoa_prealloc(const itoa_prealloc&) = delete;

        template<typename type>
        uint32_t write(type value, char* buf) const
        {
            assert(value < values);
            memcpy(buf, &data[value * digits], digits);
            return digits;
        }
    };

    template<typename first, typename second>
    struct itoa_combined
    {
        const first& f;
        const second& s;
        static const uint32_t digits = first::digits + second::digits;
        static const uint64_t values = pow10<digits>::result;

        itoa_combined(const first& f, const second& s) : f(f), s(s)
        {
        }

        template<typename type>
        uint32_t write(type value, char* buf) const
        {
            assert(value < values);
            buf += f.write(value / s.values, buf);
            s.write(value % s.values, buf);
            return digits;
        }
    };

    typedef itoa_prealloc<1> pit1; typedef itoa_prealloc<2> pit2;
    typedef itoa_prealloc<3> pit3; typedef itoa_prealloc<4> pit4;
    typedef itoa_combined<pit4, pit1> pit5; typedef itoa_combined<pit4, pit2> pit6;
    typedef itoa_combined<pit4, pit3> pit7; typedef itoa_combined<pit4, pit4> pit8;
    typedef itoa_combined<pit8, pit1> pit9; typedef itoa_combined<pit8, pit2> pit10;

    const pit1 ita1; const pit2 ita2;
    const pit3 ita3; const pit4 ita4;
    const pit5 ita5(ita4, ita1); const pit6 ita6(ita4, ita2);
    const pit7 ita7(ita4, ita3); const pit8 ita8(ita4, ita4);
    const pit9 ita9(ita8, ita1); const pit10 ita10(ita8, ita2);

    inline uint32_t log10(uint32_t i)
    {
        if(i <= 9999) {
            if(i <= 99) {
                if(i <= 9)
                    return i ? 1 : 0;
                else
                    return 2;
            }
            else
                return (i <= 999) ? 3 : 4;
        }
        else{ /* >= 10000 */
            if(i <= 999999)
                return (i <= 99999) ? 5 : 6;
            else {
                if (i <= 99999999)
                    return ( i <= 9999999 ) ? 7 : 8;
                else
                    return ( i <= 999999999 ) ? 9 : 10;
            }
        }
    }

    uint32_t utoa16(char* buf, uint16_t i)
    {
        return
            (i < ita2.values ?
                (i < ita1.values ? ita1.write(i, buf) : ita2.write(i, buf)) :
                (i < ita4.values ?
                    (i < ita3.values ? ita3.write(i, buf) : ita4.write(i, buf)) :
                    ita5.write(i, buf)
                )
            );
    }

    uint32_t utoa32(char* buf, uint32_t i)
    {
        if(i <= std::numeric_limits<uint16_t>::max())
            return utoa16(buf, uint16_t(i));

        //i >= 65536 here
        return
            (i < ita6.values ?
                (i < ita5.values ? ita5.write(i, buf) : ita6.write(i, buf)) :
                (i < ita8.values ?
                    (i < ita7.values ? ita7.write(i, buf) : ita8.write(i, buf)) :
                    (i < ita9.values ? ita9.write(i, buf) : ita10.write(i, buf))
                )
            );
    }

    uint32_t utoa64(char* buf, uint64_t i)
    {
        if(i <= std::numeric_limits<uint32_t>::max())
            return utoa32(buf, uint32_t(i));

        //i >= 4294967296 here :D
        uint32_t ret = utoa64(buf, i / ita8.values);
        buf += ret;
        ita8.write(i % ita8.values, buf);
        buf += ita8.digits;
        return ret + ita8.digits;
    }

    uint32_t itoa16(char* buf, int16_t i)
    {
        if(i < 0) {
            *buf++ = '-';
            return utoa16(buf, uint16_t(-i)) + 1;
        }
        else
            return utoa16(buf, uint16_t(i));
    }

    uint32_t itoa32(char* buf, int32_t i)
    {
        if(i < 0) {
            *buf++ = '-';
            return utoa32(buf, uint32_t(-i)) + 1;
        }
        else
            return utoa32(buf, uint32_t(i));
    }

    uint32_t itoa64(char* buf, int64_t i)
    {
        if(i < 0) {
            *buf++ = '-';
            return utoa64(buf, uint64_t(-i)) + 1;
        }
        else
            return utoa64(buf, uint64_t(i));
    }

    const double d_max_d = static_cast<double>(uint64_t(1) << 62);
    uint32_t dtoa(char* buf, double v) 
    {
        if(v != v) {
            *buf++ = 'N';
            *buf++ = 'A';
            *buf = 'N';
            return 3;
        }
        if(v == std::numeric_limits<double>::infinity()) {
            *buf++ = 'I';
            *buf++ = 'N';
            *buf = 'F';
            return 3;
        }
        if(v == -std::numeric_limits<double>::infinity()) {
            *buf++ = '-';
            *buf++ = 'I';
            *buf++ = 'N';
            *buf = 'F';
            return 4;
        }
        if(std::abs(v) > d_max_d) {
            uint32_t sz = 0;
            uint32_t exp = 0;
            if(v < 0.) {
                *buf++ = '-';
                ++sz;
                v = -v;
            }
            while(v >= 10.) {
                v /= 10;
                ++exp;
            }
            uint32_t cur_sz = dtoa(buf, v);
            buf += cur_sz;
            sz += cur_sz;
            *buf++ = 'e';
            ++sz;
            sz += itoa32(buf, exp);
            return sz;
        } else {
            double intp = 0.0;
            double frac = modf(v, &intp);
            int64_t iv = int64_t(intp);
            if(iv < 0)
                iv = -iv;
            uint32_t sz = 0;
            if(v < -std::numeric_limits<double>::epsilon()) {
                *buf++ = '-';
                ++sz;
            }
            uint32_t i_sz = itoa64(buf, iv);
            sz += i_sz;
            buf += i_sz;

            if(frac < 0)
                frac = -frac;
            if(frac >= std::numeric_limits<double>::epsilon())
            {
                *buf++ = '.';
                ++sz;
                static const uint32_t biggest_mantissa = ita9.values;
                double dmantissa = biggest_mantissa * (frac + .5 / biggest_mantissa);
                uint32_t mantissa = uint32_t(dmantissa);
                if(mantissa == biggest_mantissa)
                    --mantissa;
                uint32_t is = log10(mantissa);
                if(is)
                    for(uint32_t i = is; i != 9; ++i) {
                        *buf++ = '0';
                        ++sz;
                    }

                if(mantissa && !(mantissa % 100000))
                    mantissa /= 100000;
                if(mantissa && !(mantissa % 1000))
                    mantissa /= 1000;
                while(mantissa && !(mantissa % 10))
                    mantissa /= 10;
                sz += itoa32(buf, mantissa);
            }
            return sz;
        }
    }

    void test_itoa()
    {
        char buf[1024];
        for(uint32_t i = 0; i <= 65535; ++i) {
            uint32_t sz = utoa16(buf, uint16_t(i));
            my_unused(sz);
        }
        uint32_t sz = itoa(buf, true);
        assert(std::string(&buf[0], &buf[sz]) == "1");
        sz = itoa(buf, uint16_t(1267));
        assert(std::string(&buf[0], &buf[sz]) == "1267");
        sz = itoa(buf, uint16_t(52346));
        assert(std::string(&buf[0], &buf[sz]) == "52346");
        assert(atoi<uint16_t>(buf, sz) == 52346);
        sz = itoa(buf, uint32_t(44));
        assert(std::string(&buf[0], &buf[sz]) == "44");
        assert(atoi<uint32_t>(buf, sz) == 44);
        sz = itoa(buf, uint32_t(60701));
        assert(std::string(&buf[0], &buf[sz]) == "60701");
        assert(atoi<uint32_t>(buf, sz) == 60701);
        sz = itoa(buf, uint32_t(1486666));
        assert(std::string(&buf[0], &buf[sz]) == "1486666");
        assert(atoi<uint32_t>(buf, sz) == 1486666);
        sz = itoa(buf, int16_t(-2346));
        assert(std::string(&buf[0], &buf[sz]) == "-2346");
        assert(atoi<int16_t>(buf, sz) == -2346);
        sz = itoa(buf, -52578631);
        assert(std::string(&buf[0], &buf[sz]) == "-52578631");
        assert(atoi<int32_t>(buf, sz) == -52578631);
        sz = itoa(buf, -947593471239506712LL);
        assert(std::string(&buf[0], &buf[sz]) == "-947593471239506712");
        assert(atoi<int64_t>(buf, sz) == -947593471239506712);
        sz = itoa(buf, 9475934712395012ULL);
        assert(std::string(&buf[0], &buf[sz]) == "9475934712395012");
        assert(atoi<uint64_t>(buf, sz) == 9475934712395012);
        sz = dtoa(buf, 3.03);
        assert(std::string(&buf[0], &buf[sz]) == "3.03");
        sz = dtoa(buf, -155.6999);
        assert(std::string(&buf[0], &buf[sz]) == "-155.6999");
        sz = dtoa(buf, 155.6999);
        assert(std::string(&buf[0], &buf[sz]) == "155.6999");
        sz = dtoa(buf, -155.0000001);
        assert(std::string(&buf[0], &buf[sz]) == "-155.0000001");
        sz = dtoa(buf, std::numeric_limits<double>::quiet_NaN());
        assert(std::string(&buf[0], &buf[sz]) == "NAN");
        sz = dtoa(buf, std::numeric_limits<double>::signaling_NaN());
        assert(std::string(&buf[0], &buf[sz]) == "NAN");
        sz = dtoa(buf, std::numeric_limits<double>::infinity());
        assert(std::string(&buf[0], &buf[sz]) == "INF");
        sz = dtoa(buf, -std::numeric_limits<double>::infinity());
        assert(std::string(&buf[0], &buf[sz]) == "-INF");
        my_unused(sz);
    }
}

