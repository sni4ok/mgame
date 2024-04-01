/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

struct r
{
    int32_t num;
    uint32_t den;

    r() : num(), den(1)
    {
    }
    r(int32_t num, uint32_t den) : num(num), den(den)
    {
    }
    bool operator<(const r& v) const
    {
        return int64_t(num) * v.den < int64_t(v.num) * den;
    }
    r& operator+=(const r& v);
    r operator+(const r& v) const
    {
        r ret = (*this);
        ret += v;
        return ret;
    }
    r operator-(const r& v) const
    {
        return (*this) + r(-v.num, v.den);
    }
    r operator*(const r& v) const;
    bool operator!() const
    {
        return !num;
    }
    bool operator==(const r& v) const
    {
        return int64_t(num) * v.den == int64_t(v.num) * den;
    }
    bool operator!=(const r& v) const
    {
        return !(*this == v);
    }
};

inline double to_double(const r& v)
{
    return double(v.num) / v.den;
}

template<typename stream>
stream& operator<<(stream& str, const r& v)
{
    str << v.num;
    if(v.den != 1)
        str << "/" << v.den;
    return str;
}

mstring to_string(r value);
template<> r lexical_cast<r>(const char* from, const char* to);

