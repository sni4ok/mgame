/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "myitoa.hpp"
#include "vector.hpp"

#include <cstring>

inline void my_fast_copy(const char* from, uint64_t size, char* out)
{
    memcpy(out, from, size);
}
inline void my_fast_move(const char* from, uint64_t size, char* out)
{
    memmove(out, from, size);
}
inline void my_fast_copy(const char* from, const char* to, char* out)
{
    memcpy(out, from, to - from);
}
inline void my_fast_move(const char* from, const char* to, char* out)
{
    memmove(out, from, to - from);
}

struct buf_stream
{
    char *from, *cur, *to;

    buf_stream(char* from, char* to) : from(from), cur(from), to(to)
    {
    }

    template<uint64_t size>
    buf_stream(char (&v)[size]) : from(v), cur(from), to(from + size)
    {
    }
    const char* begin() const
    {
        return from;
    }
    const char* end() const
    {
        return cur;
    }
    uint64_t size() const
    {
        return cur - from;
    }
    str_holder str() const
    {
        return str_holder(begin(), size());
    }
    bool empty() const
    {
        return cur == from;
    }
    void clear()
    {
        cur = from;
    }
    void resize(uint64_t new_sz)
    {
        uint64_t cur_sz = size();
        if(new_sz > cur_sz)
            check_size(new_sz - cur_sz);
        cur = from + new_sz;
    }
    void write(const char* v, uint64_t s)
    {
        if(unlikely(cur + s > to))
            throw str_exception("buf_stream::write() overloaded");
        my_fast_copy(v, s, cur);
        cur += s;
    }
    buf_stream& operator<<(char c)
    {
        check_size(1);
        *cur = c;
        ++cur;
        return *this;
    }
    void put(char c)
    {
        (*this) << c;
    }
    void check_size(uint64_t delta) const
    {
        if(unlikely(cur + delta > to))
            throw str_exception("buf_stream::check_size() overloaded");
    }

    template<typename type>
    typename std::enable_if_t<std::is_integral<type>::value, buf_stream&> operator<<(type t)
    {
        check_size(my_cvt::atoi_size<type>::value);
        cur += my_cvt::itoa(cur, t);
        return *this;
    }

    buf_stream& operator<<(double d)
    {
        check_size(30);
        cur += my_cvt::dtoa(cur, d);
        return *this;
    }
};

template<uint32_t stack_sz>
struct buf_stream_fixed : buf_stream
{
    char buf[stack_sz];

    buf_stream_fixed() : buf_stream(buf)
    {
    }
};

inline str_holder _str_holder(const char* str)
{
    return str_holder(str, strlen(str));
}

typedef buf_stream_fixed<16 * 1024> my_stream;

struct es : my_stream
{
    template<typename type>
    es& operator % (const type& t)
    {
        (*this) << t;
        return *this;
    }

    operator str_holder()
    {
        return str();
    }
};

void throw_system_failure(str_holder msg);

template<uint32_t stack_sz = 252>
class my_basic_string
{
    char buf[stack_sz];
    uint32_t sz;

public:
    typedef char value_type;
    typedef const char* const_iterator;
    typedef char* iterator;
    typedef char& reference;
    typedef const char& const_reference;

    my_basic_string() : sz()
    {
    }
    my_basic_string(const char* from, uint32_t size)
    {
        if(unlikely(size > stack_sz - 1))
            throw mexception(es() % "my_basic_string, impossible construct from c_str, sz: " % size % ", stack_sz: " % stack_sz);
        sz = size;
        my_fast_copy(from, sz, begin());
    }
    my_basic_string(const char* from, const char* to) : my_basic_string(from, to - from)
    {
    }
    my_basic_string(const str_holder& str) : my_basic_string(str.str, str.size)
    {
    }

    template<uint32_t sz>
    my_basic_string(const my_basic_string<sz>& r) : my_basic_string(r.begin(), r.size())
    {
    }

    template<uint32_t sz>
    my_basic_string(const char (&str)[sz]) : my_basic_string(str, sz - 1)
    {
    }

    bool operator==(str_holder s)
    {
        return str() == s;
    }
    bool operator==(const my_basic_string& r) const
    {
        if(sz == r.sz)
            return !memcmp(buf, r.buf, sz);
        return false;
    }
    bool operator!=(const my_basic_string& r) const
    {
        return !(*this == r);
    }
    const char* c_str() const
    {
        const_cast<char&>(buf[sz]) = char();
        return &buf[0];
    }
    uint32_t size() const
    {
        return sz; 
    }
    static uint32_t capacity()
    {
        return stack_sz - 1; 
    }
    bool empty() const
    {
        return !sz;
    }
    void resize(uint32_t new_sz)
    {
        if(unlikely(new_sz > stack_sz - 1))
            throw mexception(es() % "my_basic_string, impossible resize, new_sz: " % new_sz % ", stack_sz: " % stack_sz);
        sz = new_sz;
    }
    void push_back(char c)
    {
        if(unlikely(sz > stack_sz - 2))
            throw mexception(es() % "my_basic_string, impossible push_back, sz: " % sz % ", stack_sz: " % stack_sz);
        buf[sz] = c;
        ++sz;
    }
    void clear()
    {
        sz = 0;
    }
    iterator begin()
    {
        return &buf[0];
    }
    const_iterator begin() const
    {
        return &buf[0];
    }
    iterator end(){
        return &buf[sz];
    }
    const_iterator end() const
    {
        return &buf[sz];
    }
    void erase(iterator from, iterator to)
    {
        uint32_t delta = to - from;
        my_fast_move(to, end(), from);
        sz -= delta;
    }
    iterator insert(iterator it, char c)
    {
        if(unlikely(sz > stack_sz - 2))
            throw mexception(es() % "my_basic_string, impossible insert_1, sz: " % sz % ", stack_sz: " % stack_sz);
        my_fast_move(it, end(), it + 1);
        *it = c;
        ++sz;
        return it;
    }
    void insert(iterator it, iterator from, iterator to)
    {
        uint32_t delta_sz = to - from;
        if(unlikely(sz + delta_sz > stack_sz - 1))
            throw mexception(es() % "my_basic_string, impossible insert_2, sz: " % sz % ", delta_sz: " % delta_sz % ", stack_sz: " % stack_sz);
        my_fast_move(it, end(), it + delta_sz);
        my_fast_copy(from, to, it);
        sz += delta_sz;
    }
    const char& operator[](uint32_t i) const
    {
        return buf[i];
    }
    char& operator[](uint32_t i)
    {
        return buf[i];
    }
    const char& front() const
    {
        if(unlikely(!sz))
            throw str_exception("my_basic_string try to call front() for empty string");
        return buf[0];
    }
    const char& back() const
    {
        if(unlikely(!sz))
            throw str_exception("my_basic_string try to call back() for empty string");
        return buf[sz - 1];
    }
    bool operator<(const my_basic_string& r) const
    {
        return lexicographical_compare(begin(), end(), r.begin(), r.end());
    }
    bool operator<(const str_holder& s) const
    {
        return lexicographical_compare(begin(), end(), s.str,  s.str + s.size);
    }
    str_holder str() const
    {
        return str_holder(buf, sz);
    }
};

template<typename stream, uint32_t stack_sz>
stream& operator<<(stream& str, const my_basic_string<stack_sz>& v)
{
    str.write(v.begin(), v.size());
    return str;
}

typedef my_basic_string<> my_string;

