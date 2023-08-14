/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"

#include <cassert>
#include <algorithm>

template<typename type, typename comp = std::less<type>, template <typename> typename vector = mvector>
struct fset
{
    typedef type key_type;
    typedef vector<type>::value_type value_type;
    typedef vector<type>::iterator iterator;
    typedef vector<type>::const_iterator const_iterator;
    typedef vector<type>::reverse_iterator reverse_iterator;

    fset() {
    }
    fset(fset&& r) : data(r.data) {
        data = r.data;
    }
    fset(const type* from, const type* to): data(from, to) {
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
    const_iterator lower_bound(const type& k) const {
        return std::lower_bound(data.begin(), data.end(), k, comp());
    }
    const_iterator upper_bound(const type& k) const {
        return std::upper_bound(data.begin(), data.end(), k, comp());
    }
    const_iterator find(const type& k) const {
        auto ie = data.end();
        auto it = std::lower_bound(data.begin(), ie, k, comp());
        if(it != ie && !comp()(*it, k) && !comp()(k, *it))
            return it;
        return ie;
    }
    iterator find(const type& k) {
        return const_cast<value_type*>(const_cast<const fset*>(this)->find(k));
    }
    const_iterator begin() const {
        return data.begin();
    }
    iterator begin() {
        return data.begin();
    }
    const_iterator end() const {
        return data.end();
    }
    iterator end() {
        return data.end();
    }
    reverse_iterator rbegin() const
    {
        return data.rbegin();
    }
    reverse_iterator rend() const
    {
        return data.rend();
    }
    void swap(fset& r) {
        data.swap(r.data);
    }
    void swap(vector<type>& r) {
        data.swap(r);
    }
    bool operator==(const fset& r) const {
        return data == r.data;
    }
    bool operator!=(const fset& r) const {
        return data != r.data;
    }
    iterator insert(iterator it, const value_type& k) {
        if(it != data.end())
        {
            if(*it == k)
                return it;
            assert(comp()(k, *it));
        }
        return data.insert(it, k);
    }
    iterator insert(const value_type& k) {
        iterator it = std::lower_bound(data.begin(), data.end(), k, comp());
        return insert(it, k);
    }
    void erase(const type& k) {
        iterator it = find(k);
        if(it != data.end())
            data.erase(it);
    }
    iterator erase(const_iterator it) {
        data.erase(const_cast<iterator>(it));
        return iterator(it);
    }
    void reserve(uint32_t new_capacity) {
        data.reserve(new_capacity);
    }

protected:
    vector<type> data;
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

template<typename type, typename comp = std::less<type> >
struct pset : fset<type, comp, pvector>
{
    typedef fset<type, comp, pvector> base;
    using base::fset;

    base::iterator insert(base::iterator it, type* k)
    {
        if(it != this->data.end())
        {
            if(*it == *k)
            {
                delete k;
                return it;
            }
            assert(comp()(*k, *it));
        }
        return this->data.insert(it, k);
    }
    base::iterator insert(type* k)
    {
        return insert(std::lower_bound(this->data.begin(), this->data.end(), *k, comp()), k);
    }
};

