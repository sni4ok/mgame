/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mstring.hpp"
#include "string.hpp"

mexception::mexception(str_holder str)
    : msg(str.begin(), str.end())
{
    msg.push_back(char());
}

char_cit mexception::what() const noexcept
{
    return msg.begin();
}

char_cit mstring::c_str() const
{
    mstring* p = const_cast<mstring*>(this);
    p->push_back(char());
    p->pop_back();
    return begin();
}

mstring& mstring::operator+=(str_holder r)
{
    insert(r.begin(), r.end());
    return *this;
}

mstring& mstring::operator+=(const mstring& r)
{
    insert(r.begin(), r.end());
    return *this;
}

mstring mstring::operator+(str_holder r) const
{
    mstring ret(*this);
    ret.insert(r.begin(), r.end());
    return ret;
}

mstring mstring::operator+(const mstring& r) const
{
    mstring ret(*this);
    ret.insert(r.begin(), r.end());
    return ret;
}

mstring& mstring::operator+=(char c)
{
    push_back(c);
    return *this;
}

mstring mstring::operator+(char c) const
{
    mstring ret(*this);
    ret.push_back(c);
    return ret;
}

mstring operator+(str_holder l, str_holder r)
{
    mstring ret;
    ret.resize(l.size() + r.size());
    memcpy(ret.begin(), l.begin(), l.size());
    memcpy(ret.begin() + l.size(), r.begin(), r.size());
    return ret;
}

char_cit __find(char_cit from, char_cit to, char c)
{
    const void* p = __builtin_memchr(from, c, to - from);
    if(p)
        return (char_cit)p;
    else
        return to;
}

char_cit find(char_cit from, char_cit to, char c)
{
    return __find(from, to, c);
}

char_it find(char_it from, char_it to, char c)
{
    return (char_it)__find(from, to, c);
}

bool operator==(str_holder l, str_holder r)
{
    if(l.size() != r.size())
        return false;

    return !(memcmp(l.begin(), r.begin(), l.size()));
}

bool operator==(const mstring& l, str_holder r)
{
    return l.str() == r;
}

template<typename type>
void split(mvector<type>& ret, char_cit it, char_cit ie, char sep)
{
    if(it != ie)
    {
        for(;;)
        {
            char_cit i = find(it, ie, sep);
            ret.push_back(type(it, i));

            if(i == ie)
                break;

            it = i + 1;
        }
    }
}

mvector<mstring> split_s(str_holder str, char sep)
{
    mvector<mstring> ret;
    split(ret, str.begin(), str.end(), sep);
    return ret;
}

mvector<str_holder> split(str_holder str, char sep)
{
    mvector<str_holder> ret;
    split(ret, str.begin(), str.end(), sep);
    return ret;
}

mstring join(const mstring* it, const mstring* ie, char sep)
{
    mstring ret;

    if(it == ie)
        return ret;

    u64 sz = 0;
    for(auto v = it; v != ie; ++v)
        sz += v->size();

    ret.resize(sz + (ie - it) - 1);
    buf_stream str(&ret[0], &ret[0] + ret.size());

    for(auto v = it; v != ie; ++v)
    {
        if(v != it)
            str << sep;
        str << *v;
    }

    return ret;
}

mstring join(const mvector<mstring>& s, char sep)
{
    return join(s.begin(), s.end(), sep);
}

