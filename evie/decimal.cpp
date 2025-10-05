#include "time.hpp"
#include "mlog.hpp"
#include "utils.hpp"
#include "math.hpp"

static const i64 day_s = 24 * 3600, year_s = day_s * 365;

mlog& operator<<(mlog& m, print_t t)
{
    ttime_t ta = abs(t.value);

    if(ta >= seconds(year_s))
        m << div_int(to_decimal<p2>(t.value), year_s) << "years";
    else if(ta >= seconds(day_s))
        m << div_int(to_decimal<p2>(t.value), day_s) << "days";
    else if(ta >= seconds(3600))
        m << div_int(to_decimal<p2>(t.value), 3600) << "hours";
    else if(ta >= seconds(1))
        m << to_decimal<p2>(t.value) << "sec";
    else if(ta >= milliseconds(1))
        m << div_int(p2(to_us(t.value)), 10) << "ms";
    else if(ta >= microseconds(1))
        m << to_decimal<p2>(milliseconds(t.value.value)) << "us";
    else
        m << t.value.value << "ns";
    return m;
}

void test_print_t_impl(mlog& m, ttime_t v, auto ... args)
{
    m << print_t(v) << " " << print_t(-v) << " " << print_t(div_int(v, 2)) << " ";
    if constexpr(sizeof... (args))
        test_print_t_impl(m, args...);
}

void test_print_t()
{
    ttime_t v(123), v2 = v + microseconds(10), v3 = v2 + milliseconds(10),
        v4 = v3 + seconds(10), v5 = v4 + seconds(4000), v6 = v5 + seconds(36 * 3600),
        v7 = v6 + seconds(year_s * 1.2);

    mlog m;
    test_print_t_impl(m, v, v2, v3, v4, v5, v6, v7);
}

template<typename stream>
stream& operator<<(stream& s, i128d v)
{
    if(v < i128d())
    {
        s << "-";
        v = -v;
    }
    auto f = [](double v)
    {
        int p = 0;
        while(v >= 10.)
        {
            v /= 10;
            ++p;
        }
        return pair<i32, i32>(i32(v), p);
    };

    auto decimal_pow = [](double v, u32 e)
    {
        while(e > 19)
        {
            v = v * cvt::pow[19];
            e -= 19;
        }
        v = v * cvt::pow[e];
        return v;
    };

    i32 sz = 0;
rep:
    while(v >= i128d(i64(10)))
    {
        auto p = f(v.value);
        double d = v.value - decimal_pow(p.first, p.second);
        assert(d >= double());

        s << uint_fix(p.first, sz ? sz - p.second : 1, true);
        v = i128d(d);
        sz = p.second;
        goto rep;
    }
    s << uint_fix(v.value, sz ? sz : 1, true);
    return s;
}

mlog& operator<<(mlog& s, i128d v)
{
    return operator<< <mlog>(s, v);
}

mstring to_string(i128d v)
{
    buf_stream_fixed<80> s;
    s << v;
    return s.str();
}

template<>
i128d lexical_cast(char_cit f, char_cit t)
{
    if(t - f <= 18)
        return i128d(lexical_cast<i64>(f, t));

    i128d r;
rep:
    i64 sz = min<i64>(t - f, 18);
    i64 v = lexical_cast<i64>(f, f + sz);
    if(!r)
        r = i128d(v);
    else
    {
        r = mul_int(r, cvt::pow[sz]);
        r += i128d(v);
    }
    f += sz;
    if(f != t)
        goto rep;

    return r;
}

void test_i128d()
{
    auto f = []<typename type>(type v)
    {
        mstring str = to_string(v);
        type v2 = lexical_cast<type>(str);
        double d = abs(v2.value - v.value);
        (void) d;
        assert(d <= abs(v.value) * limits<double>::epsilon);
    };

    f(i128d());
    f(i128d(limits<i64>::max));

#ifdef USE_INT128_EXT
    f(i128d(double(limits<i128>::min)));
    f(i128d(double(limits<u128>::max)));
#endif
}

