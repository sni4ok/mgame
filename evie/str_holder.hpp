/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <cstring>
#include <cstdint>

template<class it1, class it2>
bool lexicographical_compare(it1 first1, it1 last1, it2 first2, it2 last2)
{
    for (; (first1 != last1) && (first2 != last2); ++first1, ++first2)
    {
        if (*first1 < *first2)
            return true;
        if (*first2 < *first1)
            return false;
    }
    return (first1 == last1) && (first2 != last2);
}

typedef char* char_it;
typedef const char* char_cit;

struct str_holder
{
    char_cit str;
    uint64_t size;

    str_holder() : str(), size()
    {
    }
    str_holder(char_cit str, uint64_t size) : str(str), size(size)
    {
    }
    str_holder(char_cit from, char_cit to) : str(from), size(to - from)
    {
    }

    template<uint64_t sz>
    str_holder(const char (&str)[sz]) : str(str), size(sz - 1)
    {
    }
    bool operator==(const str_holder& r) const
    {
        if(size == r.size)
            return !memcmp(str, r.str, size);
        return false;
    }

    template<uint64_t sz>
    bool operator==(const char (&str)[sz]) const
    {
        return (*this) == str_holder(str);
    }
    bool operator!=(const str_holder& r) const
    {
        return !(*this == r);
    }
    bool operator<(const str_holder& r) const
    {
        return lexicographical_compare(str, str + size, r.str, r.str + r.size);
    }
    char_cit begin() const
    {
        return str;
    }
    char_cit end() const
    {
        return str + size;
    }
};

template<uint64_t sz>
str_holder from_array(const char (&str)[sz])
{
    uint64_t size = sz;
    while(size && str[size - 1] == char())
        --size;
    return {str, str + size};
}

template<typename stream>
stream& operator<<(stream& s, const str_holder& str)
{
    s.write(str.str, str.size);
    return s;
}
