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

