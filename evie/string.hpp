/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "myitoa.hpp"

#include <cstring>
#include <vector>
#include <iostream>

inline void my_fast_copy(const char* from, uint32_t size, char* out)
{
    memcpy(out, from, size);
}
inline void my_fast_move(const char* from, uint32_t size, char* out)
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

struct str_holder
{
    const char* str;
    uint32_t size;
    str_holder() : str(), size() {
    }
    str_holder(const char* str, uint32_t size) : str(str), size(size) {
    }
    template<typename array>
    explicit str_holder(const array& v, typename std::enable_if_t<std::is_array<array>::value>* = 0) {
        static_assert(sizeof(v[0]) == 1 && std::is_array<array>::value, "str_holder");
        size = sizeof(v);
        str = &v[0];
        while(size && v[size - 1] == char())
            --size;
    }
    bool operator==(const str_holder& r) const {
        if(size == r.size)
            return std::equal(str, str + size, r.str);
        return false;
    }
    bool operator!=(const str_holder& r) const {
        return !(*this == r);
    }
    bool operator<(const str_holder& r) const {
        return std::lexicographical_compare(str, str + size, r.str, r.str + r.size);
    }
};

struct buf_stream
{
    char *from, *cur, *to;
    buf_stream(std::vector<char>& v) : from(&v[0]), cur(from), to(from + v.size()) {
    }
    buf_stream(char* from, char* to) : from(from), cur(from), to(to) {
    }
    template<typename array>
    buf_stream(array& v, typename std::enable_if<std::is_array<array>::value>* = 0)
        : from(&v[0]), cur(from), to(from + sizeof(v))
    {
    }
    const char* begin() const {
        return from;
    }
    const char* end() const {
        return cur;
    }
    uint32_t size() const {
        return cur - from;
    }
    bool empty() const {
        return cur == from;
    }
    void clear() {
        cur = from;
    }
    void resize(uint32_t new_sz) {
        uint32_t cur_sz = size();
        if(new_sz > cur_sz)
            check_size(new_sz - cur_sz);
        cur = from + new_sz;
    }
    void write(const char* v, uint32_t s) {
        if(unlikely(s > to - cur))
            throw std::runtime_error("buf_stream::write() overloaded");
        my_fast_copy(v, s, cur);
        cur += s;
    }
    buf_stream& operator<<(char c) {
        check_size(1);
        *cur = c;
        ++cur;
        return *this;
    }
    void put(char c) {
        (*this) << c;
    }
    buf_stream& operator<<(const std::string& str) {
        write(str.c_str(), str.size());
        return *this;
    }
    template<typename array>
    typename std::enable_if_t<std::is_array<array>::value, buf_stream&> operator<<(const array& v) {
        static_assert(std::is_array<array>::value, "buf_stream::operator<<");
        uint32_t sz = sizeof(v) / 1;
        while(sz && v[sz - 1] == char())
            --sz;
        write(&v[0], sz);
        return *this;
    }
    void check_size(uint32_t delta) const {
        if(unlikely(delta > to - cur))
            throw std::runtime_error("buf_stream::check_size() overloaded");
    }
    template<typename T>
    typename std::enable_if_t<std::is_integral<T>::value, buf_stream&> operator<<(T t) {
        check_size(my_cvt::atoi_size<T>::value);
        cur += my_cvt::itoa(cur, t);
        return *this;
    }
    buf_stream& operator<<(double d) {
        check_size(30);
        cur += my_cvt::dtoa(cur, d);
        return *this;
    }
    buf_stream& operator<<(std::ostream& (std::ostream&)) {
        (*this) << '\n';
        return *this;
    }
};

template<uint32_t stack_sz>
struct buf_stream_fixed : buf_stream
{
    char buf[stack_sz];
    buf_stream_fixed() : buf_stream(buf) {
    }
};

inline str_holder _str_holder(const char* str)
{
    return str_holder(str, strlen(str));
}

inline str_holder _str_holder(const std::string& str)
{
    return str_holder(str.c_str(), str.size());
}

template<typename stream>
stream& operator<<(stream& s, const str_holder& str)
{
    s.write(str.str, str.size);
    return s;
}

class es
{
    buf_stream_fixed<1024> s;
public:
    template<typename t>
    es& operator%(const t& v) {
        s << v;
        return *this;
    }
    operator std::string() {
        return std::string(s.begin(), s.end());
    }
};

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

    my_basic_string() : sz() {
    }
    my_basic_string(const char* from, uint32_t size) {
        if(unlikely(size > stack_sz - 1))
            throw std::runtime_error(es() % "my_basic_string, impossible construct from c_str, sz: " % size % ", stack_sz: " % stack_sz);
        sz = size;
        my_fast_copy(from, sz, begin());
    }
    my_basic_string(const char* from, const char* to) : my_basic_string(from, to - from) {
    }
    my_basic_string(const str_holder& str) : my_basic_string(str.str, str.size) {
    }
    template<uint32_t sz>
    my_basic_string(const my_basic_string<sz>& r) : my_basic_string(r.begin(), r.size()) {
    }
    my_basic_string(const std::string& r) : my_basic_string(r.c_str(), r.size()) {
    }
    template<typename array>
    my_basic_string(const array& v, typename std::enable_if<std::is_array<array>::value >* = 0)
    {
        static_assert(sizeof(v[0]) == 1 && std::is_array<array>::value && sizeof(v) < stack_sz, "my_basic_string()");
        sz = sizeof(v);
        while(sz && v[sz - 1] == char())
            --sz;
        my_fast_copy(&v[0], sz, &buf[0]);
    }
    template<typename array>
    typename std::enable_if_t<std::is_array<array>::value, bool> operator==(const array& v) const
    {
        static_assert(sizeof(v[0]) == 1 && std::is_array<array>::value && sizeof(v) < stack_sz, "my_basic_string==");
        uint32_t arr_sz = sizeof(v);
        while(arr_sz && v[arr_sz - 1] == char())
            --arr_sz;
        if(arr_sz == sz)
            return std::equal(begin(), end(), &v[0]);
        return false;
    }
    template<typename array>
    typename std::enable_if_t<std::is_array<array>::value, bool>
    operator!=(const array& v) const {
        return !(*this == v);
    }
    bool operator==(const my_basic_string& r) const {
        if(sz == r.sz)
            return std::equal(begin(), end(), r.begin());
        return false;
    }
    bool operator!=(const my_basic_string& r) const {
        return !(*this == r);
    }
    const char* c_str() const {
        const_cast<char&>(buf[sz]) = char();
        return &buf[0];
    }
    uint32_t size() const {
        return sz; 
    }
    static uint32_t capacity() {
        return stack_sz - 1; 
    }
    bool empty() const {
        return !sz;
    }
    void resize(uint32_t new_sz) {
        if(unlikely(new_sz > stack_sz - 1))
            throw std::runtime_error(es() % "my_basic_string, impossible resize, new_sz: " % new_sz % ", stack_sz: " % stack_sz);
        sz = new_sz;
    }
    void push_back(char c) {
        if(unlikely(sz > stack_sz - 2))
            throw std::runtime_error(es() % "my_basic_string, impossible push_back, sz: " % sz % ", stack_sz: " % stack_sz);
        buf[sz] = c;
        ++sz;
    }
    void clear() {
        sz = 0;
    }
    iterator begin() {
        return &buf[0];
    }
    const_iterator begin() const {
        return &buf[0];
    }
    iterator end(){
        return &buf[sz];
    }
    const_iterator end() const {
        return &buf[sz];
    }
    void erase(iterator from, iterator to) {
        uint32_t delta = to - from;
        my_fast_move(to, end(), from);
        sz -= delta;
    }
    iterator insert(iterator it, char c) {
        if(unlikely(sz > stack_sz - 2))
            throw std::runtime_error(es() % "my_basic_string, impossible insert_1, sz: " % sz % ", stack_sz: " % stack_sz);
        my_fast_move(it, end(), it + 1);
        *it = c;
        ++sz;
        return it;
    }
    void insert(iterator it, iterator from, iterator to) {
        uint32_t delta_sz = to - from;
        if(unlikely(sz + delta_sz > stack_sz - 1))
            throw std::runtime_error(es() % "my_basic_string, impossible insert_2, sz: " % sz % ", delta_sz: " % delta_sz % ", stack_sz: " % stack_sz);
        my_fast_move(it, end(), it + delta_sz);
        my_fast_copy(from, to, it);
        sz += delta_sz;
    }
    const char& operator[](uint32_t i) const {
        return buf[i];
    }
    char& operator[](uint32_t i) {
        return buf[i];
    }
    const char& front() const {
        if(unlikely(!sz))
            throw std::runtime_error("my_basic_string try to call front() for empty string");
        return buf[0];
    }
    const char& back() const {
        if(unlikely(!sz))
            throw std::runtime_error("my_basic_string try to call back() for empty string");
        return buf[sz - 1];
    }
    bool operator<(const my_basic_string& r) const {
        return std::lexicographical_compare(begin(), end(), r.begin(), r.end());
    }
    bool operator<(const str_holder& s) const {
        return std::lexicographical_compare(begin(), end(), s.str,  s.str + s.size);
    }
    str_holder str() const {
        return str_holder(begin(), sz);
    }
};

template<typename array>
std::string get_std_string(const array& v)
{
    static_assert(std::is_array<array>::value, "get_std_string");
    uint32_t sz = sizeof(v);
    while(sz && v[sz - 1] == char())
        --sz;
    return std::string(&v[0], &v[sz]);
}

template<typename stream, uint32_t stack_sz>
stream& operator<<(stream& str, const my_basic_string<stack_sz>& v)
{
    str.write(v.begin(), v.size());
    return str;
}

typedef my_basic_string<> my_string;

template<uint32_t stack_sz>
std::string get_std_string(const my_basic_string<stack_sz>& v)
{
    return std::string(v.begin(), v.end());
}

inline const std::string& get_std_string(const std::string& v)
{
    return v;
}

inline std::string get_std_string(str_holder v)
{
    return std::string(v.str, v.str + v.size);
}

