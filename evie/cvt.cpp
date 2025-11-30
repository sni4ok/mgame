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
        u32 sz = sizeof(buf) - 1 - h.size();
        if(sz > m.size())
            sz = m.size();
        memcpy(buf + h.size(), m.begin(), sz);
        buf[h.size() + sz] = char();
    }

    char_cit exception::what() const noexcept
    {
        return buf;
    }

    template<u32 digits_s>
    struct itoa_prealloc
    {
        static const u32 digits = digits_s;
        static const u32 values = pow10<digits>::result;
        char data[values * digits + 1];

        itoa_prealloc()
        {
            char tmp[digits];
            memset(tmp, '0', digits);
            char_it ptr = data;
            for(u32 i = 0; i != values; ++i)
            {
                memcpy(ptr, tmp, digits);
                ptr += digits;

                if(i == values - 1)
                    break;

                u32 cur_idx = digits - 1;

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
        u32 write(type value, char_it buf) const
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
        static const u32 digits = first::digits + second::digits;
        static const u64 values = pow10<digits>::result;

        itoa_combined(const first& f, const second& s) : f(f), s(s)
        {
        }

        template<typename type>
        u32 write(type value, char_it buf) const
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

    u32 log10(u32 i)
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

    u32 itoa(char_it buf, bool t)
    {
        buf[0] = (t ? '1' : '0');
        return 1;
    }

    u32 itoa(char *buf, char v)
    {
        *buf = v;
        return 1;
    }

    u32 itoa(char_it buf, u8 i)
    {
        return
            (i < ita2.values ?
                (i < ita1.values ? ita1.write(i, buf) : ita2.write(i, buf)) :
                    ita3.write(i, buf)
            );
    }

    u32 itoa(char_it buf, u16 i)
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

    u32 itoa(char_it buf, u32 i)
    {
        if(i <= limits<u16>::max)
            return itoa(buf, u16(i));

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

    u32 itoa(char_it buf, u64 i)
    {
        if(i <= limits<u32>::max)
            return itoa(buf, u32(i));

        //i > 4294967296 here :D
        u32 ret = itoa(buf, i / ita8.values);
        buf += ret;
        ita8.write(i % ita8.values, buf);
        buf += ita8.digits;
        return ret + ita8.digits;
    }

    u32 itoa(char_it buf, i8 i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, u8(-i)) + 1;
        }
        else
            return itoa(buf, u8(i));
    }

    u32 itoa(char_it buf, i16 i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, u16(-i)) + 1;
        }
        else
            return itoa(buf, u16(i));
    }

    u32 itoa(char_it buf, i32 i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, u32(-i)) + 1;
        }
        else
            return itoa(buf, u32(i));
    }

    u32 itoa(char_it buf, i64 i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, u64(-i)) + 1;
        }
        else
            return itoa(buf, u64(i));
    }

#ifdef USE_INT128_EXT

    u32 itoa(char *buf, u128 i)
    {
        if(i <= limits<u64>::max)
            return itoa(buf, u64(i));

        //i > 18446744073709551615
        u32 ret = itoa(buf, i / ita19.values);
        buf += ret;
        ita19.write(i % ita19.values, buf);
        buf += ita19.digits;
        return ret + ita19.digits;
    }

    u32 itoa(char_it buf, i128 i)
    {
        if(i < 0)
        {
            *buf++ = '-';
            return itoa(buf, u128(-i)) + 1;
        }
        else
            return itoa(buf, u128(i));
    }

#endif

    const double d_max_d = static_cast<double>(u64(1) << 62);

    u32 itoa(char_it buf, double v) 
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
            u32 sz = 0;
            u32 exp = 0;

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

            u32 cur_sz = itoa(buf, v);
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
            i64 iv = i64(intp);

            if(iv < 0)
                iv = -iv;

            u32 sz = 0;
            if(v < -limits<double>::epsilon)
            {
                *buf++ = '-';
                ++sz;
            }

            u32 i_sz = itoa(buf, iv);
            sz += i_sz;
            buf += i_sz;

            if(frac < 0)
                frac = -frac;
            if(frac >= limits<double>::epsilon)
            {
                *buf++ = '.';
                ++sz;
                static const u32 biggest_mantissa = ita9.values;
                double dmantissa = biggest_mantissa * (frac + .5 / biggest_mantissa);
                u32 mantissa = u32(dmantissa);
                if(mantissa == biggest_mantissa)
                    --mantissa;
                u32 is = log10(mantissa);
                if(is)
                {
                    for(u32 i = is; i != 9; ++i)
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

        u32 sz = itoa(buf, true);
        unused(sz);

        auto check = [&](str_holder s)
        {
            unused(s);
            assert(str_holder(buf, sz) == s);
        };

        check("1");
        sz = itoa(buf, u16(1267));
        check("1267");
        sz = itoa(buf, u16(52346));
        check("52346");
        assert(atoi<u16>(buf, sz) == 52346);
        sz = itoa(buf, u32(44));
        check("44");
        assert(atoi<u32>(buf, sz) == 44);
        sz = itoa(buf, u32(60701));
        check("60701");
        assert(atoi<u32>(buf, sz) == 60701);
        sz = itoa(buf, u32(1486666));
        check("1486666");
        assert(atoi<u32>(buf, sz) == 1486666);
        sz = itoa(buf, i16(-2346));
        check("-2346");
        assert(atoi<i16>(buf, sz) == -2346);
        sz = itoa(buf, -52578631);
        check("-52578631");
        assert(atoi<i32>(buf, sz) == -52578631);
        sz = itoa(buf, i64(-947593471239506712LL));
        check("-947593471239506712");
        assert(atoi<i64>(buf, sz) == -947593471239506712);
        sz = itoa(buf, u64(9475934712395012ULL));
        check("9475934712395012");
        assert(atoi<u64>(buf, sz) == 9475934712395012);

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
                u32 sz = itoa(buf, i);
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

        check_itoa(i8());
        check_itoa(i16());
        check_itoa(i32());
        check_itoa(i64());

#ifdef USE_INT128_EXT
        check_itoa(i128());
#endif
    }
}

template<>
double lexical_cast<double>(char_cit from, char_cit to)
{
    u32 size = to - from;
    if(!size) [[unlikely]]
        throw mexception(es() % "lexical_cast<double>() error for: " % str_holder(from, to));

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
        i64 v = cvt::atoi<i64>(from, d - from);
        u32 frac_sz = to - (d + 1);

        if(frac_sz > 19) [[unlikely]]
            throw mexception(es() % "lexical_cast<double>() error for: " % str_holder(from, to));

        u64 frac = cvt::atoi<u64>(d + 1, frac_sz);
        double f = double(frac);
        if(m)
            f = -f;
        return double(v) + f / cvt::pow[frac_sz];
    }
    if(e)
    {
        i64 mantissa = cvt::atoi<i64>(from, d - from);
        u64 exponent = cvt::atoi<u64>(d + 1, to - (d + 1));
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

    i64 v = cvt::atoi<i64>(from, size);
    return double(v);
}

