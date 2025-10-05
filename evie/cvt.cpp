/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "cvt.hpp"
#include "math.hpp"
#include "string.hpp"
#include "algorithm.hpp"

namespace cvt
{
    exception::exception(str_holder h, str_holder m)
    {
        memcpy(buf, h.begin(), h.size());
        uint32_t sz = sizeof(buf) - 1 - h.size();
        if(sz > m.size())
            sz = m.size();
        memcpy(buf + h.size(), m.begin(), sz);
        buf[h.size() + sz] = char();
    }

    char_cit exception::what() const noexcept
    {
        return buf;
    }

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
            char_it ptr = data;
            for(uint32_t i = 0; i != values; ++i)
            {
                memcpy(ptr, tmp, digits);
                ptr += digits;

                if(i == values - 1)
                    break;

                uint32_t cur_idx = digits - 1;

                for(;;)
                {
                    if(tmp[cur_idx] < '9')
                    {
                        ++tmp[cur_idx];
                        break;
                    }
                    else
                    {
                        tmp[cur_idx] = '0';
                        --cur_idx;
                    }
                }
            }
        }

        itoa_prealloc(const itoa_prealloc&) = delete;

        template<typename type>
        uint32_t write(type value, char_it buf) const
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
        uint32_t write(type value, char_it buf) const
        {
            assert(value < values);
            buf += f.write(value / s.values, buf);
            s.write(value % s.values, buf);
            return digits;
        }
    };

    typedef itoa_prealloc<1> pit1; typedef itoa_prealloc<2> pit2;
    typedef itoa_prealloc<3> pit3; typedef itoa_prealloc<4> pit4;
    typedef itoa_prealloc<5> pit5; typedef itoa_combined<pit4, pit2> pit6;
    typedef itoa_combined<pit4, pit3> pit7; typedef itoa_combined<pit4, pit4> pit8;
    typedef itoa_combined<pit5, pit4> pit9; typedef itoa_combined<pit5, pit5> pit10;
    typedef itoa_combined<pit10, pit9> pit19;

    const pit1 ita1; const pit2 ita2;
    const pit3 ita3; const pit4 ita4;
    const pit5 ita5; const pit6 ita6(ita4, ita2);
    const pit7 ita7(ita4, ita3); const pit8 ita8(ita4, ita4);
    const pit9 ita9(ita5, ita4); const pit10 ita10(ita5, ita5);
    const pit19 ita19(ita10, ita9);

    uint32_t log10(uint32_t i)
    {
        if(i <= 9999)
        {
            if(i <= 99)
            {
                if(i <= 9)
                    return i ? 1 : 0;
                else
                    return 2;
            }
            else
                return (i <= 999) ? 3 : 4;
        }
        else /* >= 10000 */
        {
            if(i <= 999999)
                return (i <= 99999) ? 5 : 6;
            else
            {
                if (i <= 99999999)
                    return ( i <= 9999999 ) ? 7 : 8;
                else
                    return ( i <= 999999999 ) ? 9 : 10;
            }
        }
    }

    uint32_t itoa(char_it buf, bool t)
    {
        buf[0] = (t ? '1' : '0');
        return 1;
    }

    uint32_t itoa(char *buf, char v)
    {
        *buf = v;
        return 1;
    }

    uint32_t itoa(char_it buf, uint8_t i)
    {
        return
            (i < ita2.values ?
                (i < ita1.values ? ita1.write(i, buf) : ita2.write(i, buf)) :
                    ita3.write(i, buf)
            );
    }

    uint32_t itoa(char_it buf, uint16_t i)
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

    uint32_t itoa(char_it buf, uint32_t i)
    {
        if(i <= limits<uint16_t>::max)
            return itoa(buf, uint16_t(i));

        //i > 65536 here
        return
            (i < ita6.values ?
                (i < ita5.values ? ita5.write(i, buf) : ita6.write(i, buf)) :
                (i < ita8.values ?
                    (i < ita7.values ? ita7.write(i, buf) : ita8.write(i, buf)) :
                    (i < ita9.values ? ita9.write(i, buf) : ita10.write(i, buf))
                )
            );
    }

    uint32_t itoa(char_it buf, uint64_t i)
    {
        if(i <= limits<uint32_t>::max)
            return itoa(buf, uint32_t(i));

        //i > 4294967296 here :D
        uint32_t ret = itoa(buf, i / ita8.values);
        buf += ret;
        ita8.write(i % ita8.values, buf);
        buf += ita8.digits;
        return ret + ita8.digits;
    }

    uint32_t itoa(char_it buf, int8_t i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, uint8_t(-i)) + 1;
        }
        else
            return itoa(buf, uint8_t(i));
    }

    uint32_t itoa(char_it buf, int16_t i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, uint16_t(-i)) + 1;
        }
        else
            return itoa(buf, uint16_t(i));
    }

    uint32_t itoa(char_it buf, int32_t i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, uint32_t(-i)) + 1;
        }
        else
            return itoa(buf, uint32_t(i));
    }

    uint32_t itoa(char_it buf, int64_t i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, uint64_t(-i)) + 1;
        }
        else
            return itoa(buf, uint64_t(i));
    }

#ifdef USE_INT128_EXT

    uint32_t itoa(char *buf, uint128_t i)
    {
        if(i <= limits<uint64_t>::max)
            return itoa(buf, uint64_t(i));

        //i > 18446744073709551615
        uint32_t ret = itoa(buf, i / ita19.values);
        buf += ret;
        ita19.write(i % ita19.values, buf);
        buf += ita19.digits;
        return ret + ita19.digits;
    }

    uint32_t itoa(char_it buf, int128_t i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, uint128_t(-i)) + 1;
        }
        else
            return itoa(buf, uint128_t(i));
    }

#endif

    const double d_max_d = static_cast<double>(uint64_t(1) << 62);

    uint32_t itoa(char_it buf, double v) 
    {
        if(v != v)
        {
            *buf++ = 'N';
            *buf++ = 'A';
            *buf = 'N';
            return 3;
        }
        if(v == limits<double>::infinity)
        {
            *buf++ = 'I';
            *buf++ = 'N';
            *buf = 'F';
            return 3;
        }
        if(v == -limits<double>::infinity)
        {
            *buf++ = '-';
            *buf++ = 'I';
            *buf++ = 'N';
            *buf = 'F';
            return 4;
        }
        if(abs(v) > d_max_d)
        {
            uint32_t sz = 0;
            uint32_t exp = 0;

            if(v < 0.)
            {
                *buf++ = '-';
                ++sz;
                v = -v;
            }
            while(v >= 10.)
            {
                v /= 10;
                ++exp;
            }

            uint32_t cur_sz = itoa(buf, v);
            buf += cur_sz;
            sz += cur_sz;
            *buf++ = 'e';
            ++sz;
            sz += itoa(buf, exp);
            return sz;
        }
        else
        {
            double intp = 0.0;
            double frac = modf(v, &intp);
            int64_t iv = int64_t(intp);

            if(iv < 0)
                iv = -iv;

            uint32_t sz = 0;
            if(v < -limits<double>::epsilon)
            {
                *buf++ = '-';
                ++sz;
            }

            uint32_t i_sz = itoa(buf, iv);
            sz += i_sz;
            buf += i_sz;

            if(frac < 0)
                frac = -frac;
            if(frac >= limits<double>::epsilon)
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
                {
                    for(uint32_t i = is; i != 9; ++i)
                    {
                        *buf++ = '0';
                        ++sz;
                    }
                }

                if(mantissa && !(mantissa % 100000))
                    mantissa /= 100000;
                if(mantissa && !(mantissa % 1000))
                    mantissa /= 1000;
                while(mantissa && !(mantissa % 10))
                    mantissa /= 10;
                sz += itoa(buf, mantissa);
            }
            return sz;
        }
    }

    void test_itoa()
    {
        char buf[1024];

        uint32_t sz = itoa(buf, true);
        unused(sz);

        auto check = [&](str_holder s)
        {
            unused(s);
            assert(str_holder(buf, sz) == s);
        };

        check("1");
        sz = itoa(buf, uint16_t(1267));
        check("1267");
        sz = itoa(buf, uint16_t(52346));
        check("52346");
        assert(atoi<uint16_t>(buf, sz) == 52346);
        sz = itoa(buf, uint32_t(44));
        check("44");
        assert(atoi<uint32_t>(buf, sz) == 44);
        sz = itoa(buf, uint32_t(60701));
        check("60701");
        assert(atoi<uint32_t>(buf, sz) == 60701);
        sz = itoa(buf, uint32_t(1486666));
        check("1486666");
        assert(atoi<uint32_t>(buf, sz) == 1486666);
        sz = itoa(buf, int16_t(-2346));
        check("-2346");
        assert(atoi<int16_t>(buf, sz) == -2346);
        sz = itoa(buf, -52578631);
        check("-52578631");
        assert(atoi<int32_t>(buf, sz) == -52578631);
        sz = itoa(buf, int64_t(-947593471239506712LL));
        check("-947593471239506712");
        assert(atoi<int64_t>(buf, sz) == -947593471239506712);
        sz = itoa(buf, uint64_t(9475934712395012ULL));
        check("9475934712395012");
        assert(atoi<uint64_t>(buf, sz) == 9475934712395012);

        auto check_double = [&buf, &sz](double v, str_holder s)
        {
            unused(v, s);
            sz = itoa(buf, v);
            assert(str_holder(buf, sz) == s);
            double d = lexical_cast<double>(buf, buf + sz);
            unused(d);
            if(v == v)
            {
                assert(d == v);
            }
            else
            {
                assert(!memcmp(&v, &d, sizeof(double)));
            }
        };

        check_double(3.03, "3.03");
        check_double(-155.6999, "-155.6999");
        check_double(155.6999, "155.6999");
        check_double(-155.0000001, "-155.0000001");
        check_double(limits<double>::quiet_NaN, "NAN");
        sz = itoa(buf, limits<double>::signaling_NaN);
        check("NAN");
        check_double(limits<double>::infinity, "INF");
        check_double(-limits<double>::infinity, "-INF");
        double v = lexical_cast<double>("1e10");
        unused(v);
        assert(v == 10000000000.);

        auto check_i = [&]<typename type>(type)
        {
            auto c = [&]<typename t>(t i)
            {
                uint32_t sz = itoa(buf, i);
                type v = atoi<t>(buf, sz);
                assert(v == i);
                unused(sz, v);
            };

            type a = limits<type>::max, b = limits<type>::min;
            while(a != 0 || b != 0)
            {
                c(a);
                c(b);
                a /= 10;
                b /= 10;
            }
        };

        auto check_itoa = [&]<typename type>(type v)
        {
            check_i(v);
            check_i(make_unsigned_t<type>());
        };

        check_itoa(int8_t());
        check_itoa(int16_t());
        check_itoa(int32_t());
        check_itoa(int64_t());

#ifdef USE_INT128_EXT
        check_itoa(int128_t());
#endif
    }
}

template<>
double lexical_cast<double>(char_cit from, char_cit to)
{
    try
    {
        uint32_t size = to - from;
        if(!size)
            throw str_exception("lexical_cast<double>() from == to");

        bool p = false, e = false;
        char_cit d = to;

        for(char_cit it = from; it != to; ++it)
        {
            if(*it == '.')
            {
                p = true;
                d = it;
                break;
            }
            if(*it == 'e')
            {
                e = true;
                d = it;
                break;
            }
        }
        bool m = *from == '-';
        if(p)
        {
            int64_t v = cvt::atoi<int64_t>(from, d - from);
            uint32_t frac_sz = to - (d + 1);

            if(frac_sz > 19)
                throw str_exception("lexical_cast<double>() frac_sz overflow");

            uint64_t frac = cvt::atoi<uint64_t>(d + 1, frac_sz);
            double f = double(frac);
            if(m)
                f = -f;
            return double(v) + f / cvt::pow[frac_sz];
        }
        if(e)
        {
            int64_t mantissa = cvt::atoi<int64_t>(from, d - from);
            uint64_t exponent = cvt::atoi<uint64_t>(d + 1, to - (d + 1));
            return double(mantissa) * exp10(double(exponent));
        }

        if(size == 3)
        {
            if(*from == 'N' && *(from + 1) == 'A' && *(from + 2) == 'N')
                return limits<double>::quiet_NaN;
            else if(*from == 'I' && *(from + 1) == 'N' && *(from + 2) == 'F')
                return limits<double>::infinity;
        }
        if(size == 4 && m && *(from + 1) == 'I' && *(from + 2) == 'N' && *(from + 3) == 'F')
            return -limits<double>::infinity;

        int64_t v = cvt::atoi<int64_t>(from, size);
        return double(v);
    }
    catch(exception& e)
    {
        throw mexception(es() % "lexical_cast<double>() error for: " % str_holder(from, to));
    }
}

