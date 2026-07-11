/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "decimal.hpp"
#include "mlog.hpp"
#include "utils.hpp"
#include "string.hpp"
#include "mstring.hpp"

template<typename stream>
stream& operator<<(stream& s, i128d v)
{
    if(v < i128d())
    {
        s << "-";
        v.value = -v.value;
    }
    auto f = [](double v)
    {
        i32 p = 0;
        while(v >= 10.)
        {
            v /= 10;
            ++p;
        }
        return pair<i32, i32>{i32(v), p};
    };

    auto decimal_pow = [](double v, u32 e)
    {
        while(e > 19)
        {
            v = v * __pow10[19];
            e -= 19;
        }
        v = v * __pow10[e];
        return v;
    };

    u32 sz = 0;
rep:
    while(!(v < i128d(i64(10))))
    {
        auto p = f(v.value);
        double d = v.value - decimal_pow(p.first, p.second);
        ASSERT(d >= double());

        s << uint_fix{u64(p.first), sz ? sz - p.second : 1, true};
        v = i128d(d);
        sz = p.second;
        goto rep;
    }
    s << uint_fix{u64(v.value), sz ? sz : 1, true};
    return s;
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
    if(r.value == double())
        r = i128d(v);
    else
    {
        r.value *= __pow10[sz];
        r.value += v;
    }
    f += sz;
    if(f != t)
        goto rep;

    return r;
}

mstring to_string(i128d v)
{
    buf_stream_fixed<80> s;
    s << v;
    return s.str();
}

void test_i128d()
{
    auto f = []<typename type>(type v)
    {
        mstring str = to_string(v);
        mlog() << v;
        type v2 = lexical_cast<type>(str);
        double d = abs(v2.value - v.value);
        (void) d;
        ASSERT(d <= abs(v.value) * limits<double>::epsilon);
    };

    f(i128d());
    f(i128d(limits<i64>::max));

#ifdef USE_INT128_EXT
    f(i128d(double(limits<i128>::min)));
    f(i128d(double(limits<u128>::max)));
#endif
}
