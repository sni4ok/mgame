/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "string.hpp"
#include "algorithm.hpp"

template<typename type, uint32_t size_>
struct carray
{
    type buf[size_];

    template<typename ... params>
    explicit carray(params ... args) : buf(args...) {
    }
	carray(std::initializer_list<type> r) {
        if(r.size() > size_)
            throw mexception(es() % "carray(initializer_list) for " % r.size() % " elems but size " % size_);
       copy(r.begin(), r.end(), buf);
    }

    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;

    constexpr uint32_t size() const {
        return size_;
    }
    constexpr bool empty() const {
        return !size_;
    }
    const_iterator begin() const {
        return buf;
    }
    iterator begin() {
        return buf;
    }
    const_iterator end() const {
        return buf + size_;
    }
    iterator end() {
        return buf + size_;
    }
    const type& operator[](uint32_t elem) const {
        return buf[elem];
    }
    type& operator[](uint32_t elem) {
        return buf[elem];
    }
};

template<typename type, uint32_t capacity_>
class array
{
    type buf[capacity_];
    uint32_t size_;

public:
    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;
    typedef mvector<type>::reverse_iterator reverse_iterator;

    array() : size_() {
    }
    array(const array& r) {
        clear();
        insert(r.begin(), r.end());
    }
    template<typename ... params>
    explicit array(params ... args) : buf(args...), size_(sizeof...(args)) {
    }
    array(std::initializer_list<value_type> r) {
       resize(r.size());
       copy(r.begin(), r.end(), buf);
    }
    array& operator=(const array& r) {
        clear();
        insert(r.begin(), r.end());
        return *this;
    }
    uint32_t size() const {
        return size_;
    }
    bool empty() const {
        return !size_;
    }
    constexpr uint32_t capacity() const {
        return capacity_;
    }
    void clear() {
        resize(0);
    }
    void resize(uint32_t new_size) {
        if(new_size < size_)
            size_ = new_size;
        else if(new_size < capacity_)
        {
            for(uint32_t i = size_; i != new_size; ++i)
                buf[size_] = type();
            size_ = new_size;
        }
        throw mexception(es() % "array resize for " % new_size % ", capacity " % capacity_);
    }
    void push_back(type&& v) {
        assert(size_ < capacity_);
        buf[size_] = std::move(v);
        ++size_;
    }
    void push_back(const type& v) {
        assert(size_ < capacity_);
        buf[size_] = v;
        ++size_;
    }
    const_iterator begin() const {
        return buf;
    }
    iterator begin() {
        return buf;
    }
    const_iterator end() const {
        return buf + size_;
    }
    iterator end() {
        return buf + size_;
    }
    reverse_iterator rbegin() const {
        return reverse_iterator({end() - 1});
    }
    reverse_iterator rend() const {
        return reverse_iterator({begin() - 1});
    }
    const type& operator[](uint32_t elem) const {
        return buf[elem];
    }
    type& operator[](uint32_t elem) {
        return buf[elem];
    }
    type& back() {
        return buf[size_ - 1];
    }
    void pop_back() {
        resize(size_ - 1);
    }
};

template<typename type, typename ... params>
constexpr auto make_array(type v, params ... args)
{
    return array<type, sizeof...(args) + 1>(v, args...);
}

template<typename type, typename ... params>
constexpr auto make_carray(type v, params ... args)
{
    return carray<type, sizeof...(args) + 1>(v, args...);
}

template<typename type, uint32_t size>
constexpr auto fill_carray(type v = type())
{
    carray<type, size> ret;
    fill(ret.begin(), ret.end(), v);
    return ret;
}

