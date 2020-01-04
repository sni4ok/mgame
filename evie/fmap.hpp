/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"

template<typename key, typename value>
struct fmap
{
    struct pair
    {
        key first;
        value second;
        bool operator<(const pair& r) const {
            return first < r.first;
        }
    };

    typedef pair value_type;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;

    fmap() : v(){
    }
    fmap(const fmap&) = delete;
    void clear() {
        data.clear();
    }
    uint32_t size() const {
        return data.size();
    }
    bool empty() const {
        return data.empty();
    }
    value& at(key k) {
        v.first = k;
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, v);
        if(unlikely(it == ie || it->first != k))
            throw std::runtime_error("fmap::at() error");
        return it->second;
    }
    value& operator[](key k) {
        v.first = k;
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, v);
        if(it == ie || it->first != k) {
            v.second = value();
            it = data.insert(it, v);
        }
        return it->second;
    }
    const value_type* lower_bound(key k) {
        v.first = k;
        return std::lower_bound(data.begin(), data.end(), v);
    }
    const value_type* upper_bound(key k) {
        v.first = k;
        return std::upper_bound(data.begin(), data.end(), v);
    }
    value_type* find(key k) {
        v.first = k;
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, v);
        if(it != ie && it->first == k)
            return it;
        return ie;
    }
    const value_type* begin() const {
        return data.begin();
    }
    value_type* begin() {
        return data.begin();
    }
    const value_type* end() const {
        return data.end();
    }
    value_type* end() {
        return data.end();
    }
    void swap(fmap& r) {
        data.swap(r.data);
    }
    void swap(mvector<value_type>& r) {
        data.swap(r);
    }
    void erase(value_type* it) {
        data.erase(it);
    }
    
private:
    mutable value_type v;
    mvector<value_type> data;
};

