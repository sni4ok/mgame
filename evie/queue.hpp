/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"

template<typename type>
class queue 
{
    typedef mvector<type> data_type;
    data_type data;
    uint64_t from;

public:
    explicit queue(uint32_t prealloc_size = 32) : from() {
        data.reserve(prealloc_size);
    }

    typedef data_type::iterator iterator;
    typedef data_type::const_iterator const_iterator;
    typedef type value_type;
    typedef data_type::reverse_iterator reverse_iterator;

    type& front() {
        return data[from];
    }
    type& back() {
        return data.back();
    }
    void __pop_front_impl() {
        if(from * 2 >= data.capacity())
        {
            memmove((void*)data.begin(), data.begin() + from, (data.size() - from) * sizeof(type));
            memset((void*)(data.end() - from), 0, from * sizeof(type));
            data.resize(data.size() - from);
            from = 0;
        }
    }
    void pop_front(uint64_t count) {
        assert(from + count < data.size());
        from += count;
        __pop_front_impl();
    }
    void pop_front() {
        assert(from < data.size());
        ++from;
        __pop_front_impl();
    }
    void pop_back() {
        data.pop_back();
    }
    void push_back(type&& v) {
        data.emplace_back(std::move(v));
    }
    void push_back(const type& v) {
        data.push_back(v);
    }
    void clear() {
        data.clear();
        from = 0;
    }
    void swap(queue& r) {
        data.swap(r.data);
        std::swap(from, r.from);
    }
    const_iterator begin() const {
        return data.begin() + from;
    }
    iterator begin() {
        return data.begin() + from;
    }
    const_iterator end() const {
        return data.end();
    }
    iterator end() {
        return data.end();
    }
    reverse_iterator rbegin() {
        return data.rbegin();
    }
    reverse_iterator rend() {
        return data.rend() - from;
    }
    bool empty() const {
        return from == data.size();
    }
    uint64_t size() const {
        return data.size() - from;
    }
    iterator erase(iterator it) {
        return data.erase(it);
    }
};

