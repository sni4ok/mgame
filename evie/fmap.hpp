/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_FMAP_HPP
#define EVIE_FMAP_HPP

#include "string.hpp"
#include "algorithm.hpp"

template<typename key, typename value, typename comp = less<key>, template <typename> typename vector = fvector>
struct fmap
{
    typedef ::pair<key, value> pair;
    typedef key key_type;
    typedef value mapped_type;
    typedef vector<pair>::value_type value_type;
    typedef vector<pair>::iterator iterator;
    typedef vector<pair>::const_iterator const_iterator;

    vector<pair> data;

    fmap() {
    }
    fmap(fmap&& r) : data(::move(r.data)) {
    }
    fmap(const fmap& r) : data(r.data) {
    }
    fmap(std::initializer_list<value_type> init) {
        for(const auto& v: init)
            insert(v);
    }
    fmap& operator=(fmap&& r) {
        data = ::move(r.data);
        return *this;
    }
    fmap& operator=(const fmap& r) {
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
    static bool not_equal(const key& l, const key& r) {
        return comp()(l, r) || comp()(r, l);
    }
    const value& at(const key& k) const {
        auto it = lower_bound(k);
        if(it == data.end() || not_equal(it->first, k)) [[unlikely]]
            throw str_exception("fmap::at() error");
        return it->second;
    }
    const value& at(const key& k, str_holder fmap_name) const {
        auto it = lower_bound(k);
        if(it == data.end() || not_equal(it->first, k)) [[unlikely]]
            throw mexception(es() % "fmap::at() error, " % fmap_name % " unknown key: " % k);
        return it->second;
    }
    value& at(const key& k) {
        return const_cast<value&>(const_cast<const fmap*>(this)->at(k));
    }
    value& at(const key& k, str_holder fmap_name) {
        return const_cast<value&>(const_cast<const fmap*>(this)->at(k, fmap_name));
    }
    value& operator[](const key_type& k) {
        auto it = lower_bound(k);
        if(it == data.end() || not_equal(it->first, k)) {
            it = data.insert(it, value_type());
            it->first = k;
        }
        return it->second;
    }
    template<typename key_t>
    value& operator[](const key_t& k) {
        return (*this)[key_type(k)];
    }
    const_iterator lower_bound(const key& k) const {
        return ::lower_bound(data.begin(), data.end(), k,
            [](const pair& v, const key_type& k) { return comp()(v.first, k); }
        );
    }
    iterator lower_bound(const key& k) {
        return iterator(const_cast<const fmap*>(this)->lower_bound(k));
    }
    const_iterator upper_bound(const key& k) const {
        return ::upper_bound(data.begin(), data.end(), k,
            [](const key_type& k, const pair& v) { return comp()(k, v.first); }
        );
    }
    iterator upper_bound(const key& k) {
        return iterator(const_cast<const fmap*>(this)->upper_bound(k));
    }
    const_iterator find(const key& k) const {
        auto it = lower_bound(k);
        if(it == data.end() || not_equal(it->first, k))
            return data.end();
        return it;
    }
    iterator find(const key& k) {
        return iterator(const_cast<const fmap*>(this)->find(k));
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
    void swap(fmap& r) {
        data.swap(r.data);
    }
    iterator insert(const pair& v) {
        auto it = lower_bound(v.first);
        if(it == data.end() || not_equal(it->first, v.first))
            it = data.insert(it, v);
        return it;
    }
    iterator insert(pair&& v) {
        auto it = lower_bound(v.first);
        if(it == data.end() || not_equal(it->first, v.first))
            it = data.insert(it, ::move(v));
        return it;
    }
    bool erase(const key& k) {
        iterator it = find(k);
        if(it != data.end()) {
            data.erase(it);
            return true;
        }
        return false;
    }
    iterator erase(const_iterator it) {
        data.erase(iterator(it));
        return iterator(it);
    }
    void erase(const_iterator from, const_iterator to) {
        data.erase(iterator(from), iterator(to));
    }
    void reserve(uint64_t new_capacity) {
        data.reserve(new_capacity);
    }
};

template<typename key, typename value>
pair<key, value>* make_pair_ptr(const key& k)
{
    pair<key, value>* p = (pair<key, value>*)::operator new(sizeof(pair<key, value>));
    new(&p->second)value();
    new(&p->first)key(k);
    return p;
}

template<typename key, typename value>
struct pmap : fmap<key, value, less<key>, ppvector>
{
    typedef fmap<key, value, less<key>, ppvector> base;
    using base::fmap::fmap;

    base::iterator insert(base::pair* v) {
        auto it = base::lower_bound(v->first);
        if(it == this->data.end() || this->not_equal(it->first, v->first))
            it = this->data.insert(it, v);
        else
            delete v;
        return it;
    }
    value& operator[](const key& k) {
        auto it = this->lower_bound(k);
        if(it == this->data.end() || this->not_equal(it->first, k)) {
            //it = this->data.insert(it, new pair<key, value>({k, value()}));
            it = this->data.insert(it, make_pair_ptr<key, value>(k));
            it->first = k;
        }
        return it->second;
    }
};

#endif

