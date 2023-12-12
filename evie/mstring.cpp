/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mstring.hpp"

mexception::mexception(str_holder str)
{
    msg.reserve(str.size + 1);
    msg.insert(str.str, str.str + uint32_t(str.size));
    msg.push_back(char());
}

const char* mexception::what() const noexcept
{
    return msg.begin();
}

mstring::mstring()
{
    __clear();
}

mstring::mstring(str_holder str) : base(str.str, str.str + str.size)
{
}

mstring::mstring(std::initializer_list<char> r) : base(r.begin(), r.end())
{
}

mstring::mstring(const char* from, const char* to) : base(from, to)
{
}

mstring::mstring(const mstring& r) : base(static_cast<const base&>(r))
{
}

mstring::mstring(mstring&& r) : base(std::move(static_cast<base&&>(r)))
{
}

mstring& mstring::operator=(mstring&& r)
{
    base::swap(r);
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

const char* mstring::c_str() const
{
    mstring* p = const_cast<mstring*>(this);
    p->push_back(char());
    p->resize(p->size() - 1);
    return begin();
}

bool mstring::operator==(const mstring& r) const
{
    if(size() == r.size())
        return !memcmp(begin(), r.begin(), size());
    return false;
}

bool mstring::operator==(const str_holder& r) const
{
    if(r.size == size())
        return !memcmp(begin(), r.begin(), size());
    return false;
}

bool mstring::operator!=(const mstring& r) const
{
    return !(*this == r);
}

bool mstring::operator!=(const str_holder& r) const
{
    return !(*this == r);
}

bool mstring::operator<(const mstring& r) const
{
    return lexicographical_compare(begin(), begin() + size(), r.begin(), r.begin() + r.size());
}

mstring& mstring::operator+=(const mstring& r)
{
    base::insert(r.begin(), r.end());
    return *this;
}

mstring& mstring::operator+=(str_holder r)
{
    base::insert(r.begin(), r.end());
    return *this;
}

mstring mstring::operator+(const mstring& r) const
{
    mstring ret(*this);
    return ret += r;
}

mstring mstring::operator+(str_holder r) const
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
    ret.resize(l.size + r.size());
    memcpy(&ret[0], l.str, l.size);
    memcpy(&ret[0] + l.size, &r[0], r.size());
    return ret;
}

bool str_holder::operator==(const str_holder& r) const
{
    if(size == r.size)
        return !memcmp(str, r.str, size);
    return false;
}

