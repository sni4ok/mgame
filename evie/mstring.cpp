/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mstring.hpp"

mexception::mexception(str_holder str)
{
    msg.reserve(str.size() + 1);
    msg.insert(str.begin(), str.end());
    msg.push_back(char());
}

char_cit mexception::what() const noexcept
{
    return msg.begin();
}

mstring::mstring()
{
    __clear();
}

mstring::mstring(str_holder r) : base(r.begin(), r.end())
{
}

mstring::mstring(const mstring& r) : base(static_cast<const base&>(r))
{
}

mstring::mstring(std::initializer_list<char> r) : base(r.begin(), r.end())
{
}

mstring::mstring(char_cit from, char_cit to) : base(from, to)
{
}

mstring::mstring(mstring&& r) : base(move(static_cast<base&&>(r)))
{
}

mstring& mstring::operator=(mstring&& r)
{
    base::swap(r);
    return *this;
}

mstring& mstring::operator=(str_holder r)
{
    clear();
    insert(r.begin(), r.end());
    return *this;
}

mstring& mstring::operator=(const mstring& r)
{
    static_cast<base&>(*this) = r;
    return *this;
}

void mstring::swap(mstring& r)
{
    base::swap(r);
}

str_holder mstring::str() const
{
    return {begin(), size()};
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
    base::insert(r.begin(), r.end());
    return *this;
}

mstring& mstring::operator+=(const mstring& r)
{
    base::insert(r.begin(), r.end());
    return *this;
}

mstring mstring::operator+(str_holder r) const
{
    mstring ret(*this);
    return ret += r;
}

mstring mstring::operator+(const mstring& r) const
{
    mstring ret(*this);
    return ret += r;
}

mstring& mstring::operator+=(char c)
{
    push_back(c);
    return *this;
}

mstring mstring::operator+(char c) const
{
    mstring ret(*this);
    return ret += c;
}

mstring operator+(str_holder l, const mstring& r)
{
    mstring ret;
    ret.resize(l.size() + r.size());
    memcpy(ret.begin(), l.begin(), l.size());
    memcpy(ret.begin() + l.size(), r.begin(), r.size());
    return ret;
}

