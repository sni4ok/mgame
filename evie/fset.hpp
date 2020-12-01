/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"

template<typename type>
struct fset
{
    typedef type key_type;
    typedef type value_type;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;

    fset() {
    }
    fset(const fset& r) {
        data = r.data;
    }
    fset& operator=(const fset& r) {
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
    const value_type* lower_bound(type k) {
        return std::lower_bound(data.begin(), data.end(), k);
    }
    const value_type* upper_bound(type k) {
        return std::upper_bound(data.begin(), data.end(), k);
    }
    const value_type* find(type k) const {
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, k);
        if(it != ie && *it == k)
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
    void swap(fset& r) {
        data.swap(r.data);
    }
    void swap(mvector<value_type>& r) {
        data.swap(r);
    }
    void insert(type k) {
        iterator it = std::lower_bound(data.begin(), data.end(), k);
        data.insert(it, k);
    }
    void erase(value_type* it) {
        data.erase(it);
    }

private:
    mvector<value_type> data;
};

template<typename cont>
struct inserter_t
{
    cont* c;

    inserter_t(cont& c) : c(&c)
    {
    }
    inserter_t& operator+=(const typename cont::value_type& v)
    {
        c->insert(v);
        return *this;
    }
    inserter_t& operator,(const typename cont::value_type& v)
    {
        c->insert(v);
        return *this;
    }
    inserter_t& operator()(const typename cont::value_type& v)
    {
        c->insert(v);
        return *this;
    }
    template<typename key, typename value>
    inserter_t& operator()(const key& k, const value& v)
    {
        c->insert({typename cont::key_type(k), typename cont::mapped_type(v)});
        return *this;
    }
};

template<typename cont>
inserter_t<cont> inserter(cont& c)
{
    return inserter_t<cont>(c);
}

