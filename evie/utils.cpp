/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "utils.hpp"
#include "mlog.hpp"

#include <errno.h>

void throw_system_failure(const std::string& msg)
{
    throw std::runtime_error(es() % (errno ? strerror(errno) : "") % ", " % msg);
}

std::string to_string(double value)
{
    char buf[32];
    uint32_t size = my_cvt::dtoa(buf, value);
    return std::string(buf, buf + size);
}

template<>
double lexical_cast<double>(const char* from, const char* to)
{
    char* ep;
    double ret;
    if(*to == char())
        ret = strtod(from, &ep);
    else {
        my_basic_string<30> buf(from, to - from);
        ret = strtod(buf.c_str(), &ep);
        ep = (char*)(from + (ep - buf.begin()));
    }
    if(ep != to)
        throw std::runtime_error(es() % "bad lexical_cast to double from " % str_holder(from, to - from));
    return ret;
}

template<>
std::string lexical_cast<std::string>(const char* from, const char* to)
{
    return std::string(from, to);
}

std::vector<std::string> split(const std::string& str, char sep)
{
    std::vector<std::string> ret;
    auto it = str.begin(), ie = str.end(), i = it;
    while(it != ie) {
        i = std::find(it, ie, sep);
        ret.push_back(std::string(it, i));
        if(i != ie)
            ++i;
        it = i;
    }
    return ret;
}

void split(std::vector<str_holder>& ret, const char* it, const char* ie, char sep)
{
    while(it != ie) {
        const char* i = std::find(it, ie, sep);
        ret.push_back(str_holder(it, i - it));
        if(i != ie)
            ++i;
        it = i;
    }
}

class crc32_table : noncopyable
{
    uint32_t crc_table[256];
    crc32_table() {
        for(uint32_t i = 0; i != 256; ++i) {
            uint32_t crc = i;
            for (uint32_t j = 0; j != 8; j++)
                crc = crc & 1 ? (crc >> 1) ^ 0xedb88320ul : crc >> 1;
            crc_table[i] = crc;
        }
    }
public:
    static uint32_t* get() {
        static crc32_table t;
        return t.crc_table;
    }
};

crc32::crc32(uint32_t init) : crc_table(crc32_table::get()), crc(init ^ 0xFFFFFFFFUL)
{
}

void crc32::process_bytes(const char* p, uint32_t len)
{
    const unsigned char* buf = reinterpret_cast<const unsigned char*>(p);
    while(len--)
        crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
}

uint32_t crc32::checksum() const
{
    return crc ^ 0xFFFFFFFFUL;
}

template<uint32_t frac_size>
ttime_t read_time_impl::read_time(const char* &it)
{
    //2020-01-26T10:45:21 //frac_size 0
    //2020-01-26T10:45:21.418 //frac_size 3
    //2020-01-26T10:45:21.418000001 //frac_size 9

    if(unlikely(cur_date != str_holder(it, 10)))
    {
        if(*(it + 4) != '-' || *(it + 7) != '-')
            throw std::runtime_error(es() % "bad time: " % std::string(it, it + 26));
        struct tm t = tm();
        int y = my_cvt::atoi<int>(it, 4); 
        int m = my_cvt::atoi<int>(it + 5, 2); 
        int d = my_cvt::atoi<int>(it + 8, 2); 
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        cur_date_time = timegm(&t) * my_cvt::p10<9>();
        cur_date = str_holder(it, 10);
    }
    it += 10;
    if(*it != 'T' || *(it + 3) != ':' || *(it + 6) != ':' || (frac_size ? *(it + 9) != '.' : false))
        throw std::runtime_error(es() % "bad time: " % std::string(it - 10, it + 10 + (frac_size ? 1 + frac_size : 0)));
    uint64_t h = my_cvt::atoi<uint64_t>(it + 1, 2);
    uint64_t m = my_cvt::atoi<uint64_t>(it + 4, 2);
    uint64_t s = my_cvt::atoi<uint64_t>(it + 7, 2);
    uint64_t ns = 0;
    if(frac_size)
    {
        uint64_t frac = my_cvt::atoi<uint64_t>(it + 10, frac_size);
        ns = frac * my_cvt::p10<9 - frac_size>();
        it += (frac_size + 1);
    }
    it += 9;
    return ttime_t{cur_date_time + ns + (s + m * 60 + h * 3600) * ttime_t::frac};
}

template ttime_t read_time_impl::read_time<0>(const char*&);
template ttime_t read_time_impl::read_time<3>(const char*&);
template ttime_t read_time_impl::read_time<6>(const char*&);
template ttime_t read_time_impl::read_time<9>(const char*&);

namespace
{
    time_parsed parse_time_impl(const ttime_t& time)
    {
        time_parsed ret;
        time_t ti = time.value / ttime_t::frac;
        struct tm * t = gmtime(&ti);
        ret.year = t->tm_year + 1900;
        ret.month = t->tm_mon + 1;
        ret.day = t->tm_mday;
        ret.hours = t->tm_hour;
        ret.minutes = t->tm_min;
        ret.seconds = t->tm_sec;
        ret.nanos = time.value % ttime_t::frac;
        return ret;
    }

    const uint32_t cur_day_seconds = day_seconds(cur_mtime_seconds());
    const date cur_day_date = parse_time_impl(cur_mtime_seconds());

    inline my_string get_cur_day_str()
    {
        buf_stream_fixed<20> str;
        str << mlog_fixed<4>(cur_day_date.year) << "-" << mlog_fixed<2>(cur_day_date.month) << "-" << mlog_fixed<2>(cur_day_date.day);
        return my_string(str.begin(), str.end());
    }

    const my_string cur_day_date_str = get_cur_day_str();
}

mlog& mlog::operator<<(const date& d)
{
    if(d == cur_day_date)
        (*this) << cur_day_date_str;
    else
        (*this) << d.year << '-' << print2chars(d.month) << '-' << print2chars(d.day);
    return *this;
}

time_parsed parse_time(const ttime_t& time)
{
    if(day_seconds(time) == cur_day_seconds)
    {
        time_parsed ret;
        ret.date() = cur_day_date;
        uint32_t frac = (time.value / ttime_t::frac) % (24 * 3600);
        ret.seconds = frac % 60;
        ret.hours = frac / 3600;
        ret.minutes = (frac - ret.hours * 3600) / 60;
        ret.nanos = time.value % ttime_t::frac;
        return ret;
    }
    else
        return parse_time_impl(time);
}

time_duration get_time_duration(const ttime_t& time)
{
    time_duration ret;
    uint32_t frac = (time.value / time.frac) % (24 * 3600);
    ret.seconds = frac % 60;
    ret.hours = frac / 3600;
    ret.minutes = (frac - ret.hours * 3600) / 60;
    ret.nanos = time.value % time.frac;
    return ret;
}

ttime_t pack_time(const time_parsed& p)
{
    struct tm t = tm();
    t.tm_year = p.year - 1900;
    t.tm_mon = p.month - 1;
    t.tm_mday = p.day;
    t.tm_hour = p.hours;
    t.tm_min = p.minutes;
    t.tm_sec = p.seconds;
    return {uint64_t(timegm(&t) * ttime_t::frac + p.nanos)};
}

date& date::operator+=(date_duration d)
{
    time_parsed tp;
    tp.date() = *this;
    ttime_t t = pack_time(tp);
    t.value += int64_t(d.days) * 24 * 3600 * ttime_t::frac;
    tp = parse_time(t);
    *this = tp.date();
    return *this;
}

date_duration date::operator-(const date& r) const
{
    time_parsed t;
    t.date() = *this;
    ttime_t tl = pack_time(t);
    t.date() = r;
    ttime_t tr = pack_time(t);
    int64_t ns = tl - tr;
    return date_duration(ns / ttime_t::frac / (24 * 3600));
}

ttime_t time_from_date(const date& t)
{
    time_parsed p;
    p.date() = t;
    return pack_time(p);
}

