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

    typedef key key_type;
    typedef value mapped_type;
    typedef pair value_type;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;

    fmap() {
    }
    fmap(const fmap& r) {
        data = r.data;
    }
	fmap(std::initializer_list<value_type> init) {
        for(const auto& v: init)
            insert(v);
    }
    fmap& operator=(const fmap& r) {
        data = r.data;
        return *this;
    }
    void clear() {
        data.clear();
    }
    uint32_t size() const {
        return data.size();
    }
    bool empty() const {
        return data.empty();
    }
    const value& at(key k) const {
        value_type v;
        v.first = k;
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, v);
        if(unlikely(it == ie || it->first != k))
            throw std::runtime_error("fmap::at() error");
        return it->second;
    }
    value& at(key k) {
        const fmap* p = this;
        return const_cast<value&>(p->at(k));
    }
    template<typename key_t>
    value& operator[](const key_t& k) {
        value_type v;
        v.first = key_type(k);
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, v);
        if(it == ie || it->first != v.first) {
            v.second = value();
            it = data.insert(it, v);
        }
        return it->second;
    }
    const value_type* lower_bound(key k) const {
        value_type v;
        v.first = k;
        return std::lower_bound(data.begin(), data.end(), v);
    }
    const value_type* upper_bound(key k) const {
        value_type v;
        v.first = k;
        return std::upper_bound(data.begin(), data.end(), v);
    }
    const value_type* find(key k) const {
        value_type v;
        v.first = k;
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, v);
        if(it != ie && it->first == k)
            return it;
        return ie;
    }
    value_type* find(key k) {
        return const_cast<value_type*>(const_cast<const fmap*>(this)->find(k));
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
    void insert(const pair& v) {
        iterator it = std::lower_bound(data.begin(), data.end(), v);
        data.insert(it, v);
    }
    void erase(value_type* it) {
        data.erase(it);
    }

private:
    mvector<value_type> data;
};

