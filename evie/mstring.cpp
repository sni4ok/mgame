/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mstring.hpp"

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

