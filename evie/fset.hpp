/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"
#include "algorithm.hpp"

template<typename type, typename comp = less<type>, template <typename> typename vector = fvector>
struct fset
{
    typedef type key_type;
    typedef vector<type>::value_type value_type;
    typedef vector<type>::iterator iterator;
    typedef vector<type>::const_iterator const_iterator;

    vector<type> data;

    fset() {
    }
    fset(fset&& r) : data(move(r.data)) {
    }
    fset(const fset& r) : data(r.data) {
    }
    fset(std::initializer_list<type> init) {
        for(const auto& v: init)
            insert(v);
    }
    fset(span<type> str): data(str) {
    }
    fset(const type* from, const type* to): data(from, to) {
    }
    fset& operator=(fset&& r) {
        data = move(r.data);
        return *this;
    }
    fset& operator=(const fset& r) {
        data = r.data;
        return *this;
    }
    void clear() {
        data.clear();
    }
    uint64_t size() const {
        return data.size();
    }
    bool empty() const {
        return data.empty();
    }
    const_iterator lower_bound(const type& k) const {
        return ::lower_bound(data.begin(), data.end(), k, comp());
    }
    iterator lower_bound(const type& k) {
        return iterator(const_cast<const fset*>(this)->lower_bound(k));
    }
    const_iterator upper_bound(const type& k) const {
        return ::upper_bound(data.begin(), data.end(), k, comp());
    }
    iterator upper_bound(const type& k) {
        return iterator(const_cast<const fset*>(this)->upper_bound(k));
    }
    static bool not_equal(const type& l, const type& r) {
        return comp()(l, r) || comp()(r, l);
    }
    const_iterator find(const type& k) const {
        auto ie = data.end();
        auto it = ::lower_bound(data.begin(), ie, k, comp());
        if(it == ie || not_equal(*it, k))
            return ie;
        return it;
    }
    iterator find(const type& k) {
        return const_cast<type*>(const_cast<const fset*>(this)->find(k));
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
    auto rbegin() const {
        return data.rbegin();
    }
    auto rend() const {
        return data.rend();
    }
    void swap(fset& r) {
        data.swap(r.data);
    }
    iterator insert(iterator it, const type& k) {
        if(it == data.end() || not_equal(*it, k))
            return data.insert(it, k);
        return it;
    }
    iterator insert(iterator it, type&& k) {
        if(it == data.end() || not_equal(*it, k))
            return data.insert(it, move(k));
        return it;
    }
    iterator insert(const type& k) {
        iterator it = ::lower_bound(data.begin(), data.end(), k, comp());
        return insert(it, k);
    }
    iterator insert(type&& k) {
        iterator it = ::lower_bound(data.begin(), data.end(), k, comp());
        return insert(it, move(k));
    }
    iterator erase(const type& k) {
        iterator it = find(k);
        if(it != data.end())
            return data.erase(it);
        return data.end();
    }
    iterator erase(const_iterator it) {
        return data.erase(const_cast<iterator>(it));
    }
    iterator erase(iterator from, iterator to) {
        return data.erase(from, to);
    }
    void reserve(uint64_t new_capacity) {
        data.reserve(new_capacity);
    }
};

template<typename cont>
struct inserter_t
{
    cont* c;

    inserter_t(cont& c) : c(&c) {
    }
    inserter_t& operator+=(const auto& v) {
        c->insert(v);
        return *this;
    }
    inserter_t& operator,(const auto& v) {
        c->insert(v);
        return *this;
    }
    inserter_t& operator()(const auto& v) {
        c->insert(v);
        return *this;
    }
    inserter_t& operator()(const auto& k, const auto& v) {
        c->insert({typename cont::key_type(k), typename cont::mapped_type(v)});
        return *this;
    }
};

template<typename cont>
inserter_t<cont> inserter(cont& c)
{
    return inserter_t<cont>(c);
}

template<typename type, typename comp = less<type> >
struct pset : fset<type, comp, ppvector>
{
    typedef fset<type, comp, ppvector> base;
    using base::fset::fset;

    base::iterator insert(base::iterator it, type* k) {
        if(it != this->data.end()) {
            if(this->not_equal(*it, *k)) {
                assert(comp()(*k, *it));
            }
            else {
                delete k;
                return it;
            }
        }
        return this->data.insert(it, k);
    }
    base::iterator insert(type* k) {
        return insert(::lower_bound(this->data.begin(), this->data.end(), *k, comp()), k);
    }
};

