#include "time.hpp"
#include "mlog.hpp"
#include "utils.hpp"
#include "math.hpp"

static const int64_t day_s = 24 * 3600, year_s = day_s * 365;

mlog& operator<<(mlog& m, print_t t)
{
    ttime_t ta = abs(t.value);

    if(ta > seconds(year_s))
        m << div_int(to_decimal<p2>(t.value), year_s) << "years";
    else if(ta > seconds(day_s))
        m << div_int(to_decimal<p2>(t.value), day_s) << "days";
    else if(ta > seconds(3600))
        m << div_int(to_decimal<p2>(t.value), 3600) << "hours";
    else if(ta > seconds(10))
        m << to_decimal<p2>(t.value) << "sec";
    else if(ta > milliseconds(10))
        m << div_int(p2(to_us(t.value)), 10) << "ms";
    else if(ta > microseconds(10))
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
stream& operator<<(stream& s, int128d v)
{
    if(v < int128d())
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
        return pair<int32_t, int32_t>(int32_t(v), p);
    };

    auto decimal_pow = [](double v, uint32_t e)
    {
        while(e > 19)
        {
            v = v * my_cvt::pow[19];
            e -= 19;
        }
        v = v * my_cvt::pow[e];
        return v;
    };

    int32_t sz = 0;
rep:
    while(v >= int128d(int64_t(10)))
    {
        auto p = f(v.value);
        double d = v.value - decimal_pow(p.first, p.second);
        assert(d >= double());

        s << uint_fix(p.first, sz ? sz - p.second : 1, true);
        v = int128d(d);
        sz = p.second;
        goto rep;
    }
    s << uint_fix(v.value, sz ? sz : 1, true);
    return s;
}

mlog& operator<<(mlog& s, int128d v)
{
    return operator<< <mlog>(s, v);
}

mstring to_string(int128d v)
{
    buf_stream_fixed<80> s;
    s << v;
    return s.str();
}

template<>
int128d lexical_cast(char_cit f, char_cit t)
{
    if(t - f <= 18)
        return int128d(lexical_cast<int64_t>(f, t));

    int128d r;
rep:
    int64_t sz = min<int64_t>(t - f, 18);
    int64_t v = lexical_cast<int64_t>(f, f + sz);
    if(!r)
        r = int128d(v);
    else
    {
        r = mul_int(r, my_cvt::pow[sz]);
        r += int128d(v);
    }
    f += sz;
    if(f != t)
        goto rep;

    return r;
}

void test_int128d()
{
    auto f = []<typename type>(type v)
    {
        mstring str = to_string(v);
        type v2 = lexical_cast<type>(str);
        double d = abs(v2.value - v.value);
        assert(d <= abs(v.value) * limits<double>::epsilon);
    };

    f(int128d());
    f(int128d(limits<int64_t>::max));

#ifdef USE_INT128_EXT
    f(int128d(double(limits<int128_t>::min)));
    f(int128d(double(limits<uint128_t>::max)));
#endif
}

