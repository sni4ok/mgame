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
    str_holder(const char* str, uint32_t size) : str(str), size(size) {
    }
    template<typename array>
    str_holder(const array& v, typename std::enable_if_t<std::is_array<array>::value>* = 0) {
        static_assert(sizeof(v[0]) == 1 && std::is_array<array>::value, "str_holder");
        size = sizeof(v);
        str = &v[0];
        while(size && v[size - 1] == char())
            --size;
    }
    operator const char*() const {
        return str;
    }
    bool operator==(const str_holder& r) const {
        if(size == r.size)
            return std::equal(str, str + size, r.str);
        return false;
    }
};

struct _str_holder : str_holder
{
    _str_holder(const char* str) : str_holder(str, strlen(str))
    {
    }
};

template<typename ch, uint32_t stack_sz = 256>
class my_basic_string
{
    ch buf[stack_sz];
    uint32_t sz;

    void my_copy_impl(const ch* from, uint32_t size, ch* out) {
        my_fast_copy(from, size * sizeof(ch), out);
    }
    void my_move_impl(const ch* from, uint32_t size, ch* out) {
        my_fast_move(from, size * sizeof(ch), out);
    }
public:
    typedef ch value_type;

    my_basic_string() : sz() {
    }
    my_basic_string(const ch* from, uint32_t size) {
        if(unlikely(size > stack_sz - 1))
            throw std::runtime_error(es() % "my_basic_string, impossible construct from c_str, sz: " % size);
        sz = size;
        my_copy_impl(from, sz, begin());
    }
    my_basic_string(const ch* from, const ch* to) : my_basic_string(from, to - from) {
    }
    my_basic_string(const str_holder& str) : my_basic_string(str.str, str.size) {
    }
    my_basic_string(const my_basic_string& r) {
        sz = r.sz;
        my_copy_impl(r.begin(), sz, begin());
    }
    my_basic_string(const std::basic_string<ch>& r) : my_basic_string(r.c_str(), r.size()) {
    }
    my_basic_string& operator=(const my_basic_string& r) {
        sz = r.sz;
        my_copy_impl(r.begin(), sz, begin());
        return *this;
    }
    template<typename array>
    my_basic_string(const array& v, typename std::enable_if<std::is_array<array>::value >* = 0) {
        static_assert(sizeof(v[0]) == sizeof(ch) && std::is_array<array>::value
            && sizeof(v) / sizeof(ch) < stack_sz, "my_basic_string()");
        sz = sizeof(v) / sizeof(ch);
        if(v[sz - 1] == ch())
            --sz;
        my_copy_impl(&v[0], sz, &buf[0]);
        while(sz && v[sz - 1] == ch())
            --sz;
    }
    template<typename array>
    typename std::enable_if_t<std::is_array<array>::value, bool>
    operator==(const array& v) const {
        static_assert(sizeof(v[0]) == sizeof(ch) && std::is_array<array>::value
            && sizeof(v) / sizeof(ch) < stack_sz, "my_basic_string==");
        uint32_t arr_sz = sizeof(v) / sizeof(ch);
        while(arr_sz && v[arr_sz - 1] == ch())
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
    const ch* c_str() const {
        const_cast<ch&>(buf[sz]) = ch();
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
    void push_back(ch c) {
        if(unlikely(sz > stack_sz - 2))
            throw std::runtime_error(es() % "my_basic_string, impossible push_back, sz: " % sz % ", stack_sz: " % stack_sz);
        buf[sz] = c;
        ++sz;
    }
    void clear() {
        sz = 0;
    }

    typedef const ch* const_iterator;
    typedef ch* iterator;
    typedef ch& reference;
    typedef const ch& const_reference;

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
        my_move_impl(to, end() - to, from);
        sz -= delta;
    }
    iterator insert(iterator it, ch c) {
        if(unlikely(sz > stack_sz - 2))
            throw std::runtime_error(es() % "my_basic_string, impossible insert_1, sz: " % sz % ", stack_sz: " % stack_sz);
        my_move_impl(it, end() - it, it + 1);
        *it = c;
        ++sz;
        return it;
    }
    void insert(iterator it, iterator from, iterator to) {
        uint32_t delta_sz = to - from;
        if(unlikely(sz + delta_sz > stack_sz - 1))
            throw std::runtime_error(es() % "my_basic_string, impossible insert_2, sz: " % sz % ", delta_sz: " % delta_sz % ", stack_sz: " % stack_sz);
        my_move_impl(it, end() - it, it + delta_sz);
        my_copy_impl(from, to - from, it);
        sz += delta_sz;
    }
    const ch& operator[](uint32_t i) const {
        return buf[i];
    }
    ch& operator[](uint32_t i) {
        return buf[i];
    }
    const ch& front() const {
        if(unlikely(!sz))
            throw std::runtime_error("my_basic_string try to call front() for empty string");
        return buf[0];
    }
    const ch& back() const {
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

inline std::ostream& operator<<(std::ostream& s, const str_holder& str){
    s.write(str.str, str.size);
    return s;
}

struct buf_stream
{
    char *from, *cur, *to;
    buf_stream(std::vector<char>& v) : from(&v[0]), cur(from), to(from + v.size()) {
    }
    buf_stream(char* from, char* to) : from(from), cur(from), to(to) {
    }
    const char* begin() const {
        return from;
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
    void resize(uint32_t size) {
        cur = from + size;
    }
    void write(const char* v, uint32_t s) {
        if(unlikely(s > to - cur))
            throw std::runtime_error("buf_stream::write() overloaded");
        my_fast_copy(v, s, cur);
        cur += s;
    }
    void put(char c) {
        if(unlikely(1 > to - cur))
            throw std::runtime_error("buf_stream::put() overloaded");
        *cur = c;
        ++cur;
    }
    buf_stream& operator<<(char ch) {
        put(ch);
        return *this;
    }
    buf_stream& operator<<(const str_holder& str) {
        write(str.str, str.size);
        return *this;
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
    template<typename T>
    typename std::enable_if_t<std::is_integral<T>::value, buf_stream&> operator<<(T t) {
        uint32_t sz = my_cvt::itoa<T>(cur, t);
        cur += sz;
        return *this;
    }
    buf_stream& operator<<(std::ostream& (std::ostream&)) {
        put('\n');
        return *this;
    }
};

template<typename array>
inline std::string array_to_string(const array& v)
{
    static_assert(std::is_array<array>::value, "array_to_string");
    uint32_t sz = sizeof(v);
    while(sz && v[sz - 1] == char())
        --sz;
    return std::string(&v[0], &v[sz]);
}

template<typename stream, typename ch, uint32_t stack_sz>
inline stream& operator<<(stream& str, const my_basic_string<ch, stack_sz>& v){
    str.write(&v[0], v.size());
    return str;
}

typedef my_basic_string<char> my_string;
typedef my_basic_string<wchar_t> my_wstring;

template<uint32_t stack_sz>
inline std::string get_std_string(const my_basic_string<char, stack_sz>& v){
    return std::string(v.begin(), v.end());
}
inline const std::string& get_std_string(const std::string& v){
    return v;
}

