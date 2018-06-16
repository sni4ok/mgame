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
        bool operator<(const pair& r) {
            return first < r.first;
        }
    };

    typedef pair value_type;

    fmap(){
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
    value_type v;
    mvector<value_type> data;
};

