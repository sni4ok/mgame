/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "vector.hpp"

template<class It, class T, class Pr>
It lower_bound_simple(It first, It last, const T& val, Pr pred)
{
    int64_t count = last - first;
    while(count > 0)
    {
        int64_t step = count / 2;
        It mid = first + step;
        if(pred(*mid, val))
            first = ++mid, count -= step + 1;
        else
            count = step;
    }
    return first;
}

template <class It, class T, class Pr>
It upper_bound_simple(It first, It last, const T& val, Pr pred)
{
  int64_t count = last - first;
  while(count > 0)
  {
    int64_t step = count / 2;
    It mid = first + step;
    if(!pred(val, *mid))
        first = ++mid, count -= step + 1;
    else
        count = step;
  }
  return first;
}

template<typename key, typename value, typename comp = std::less<key>, template <typename> typename vector = mvector>
struct fmap
{
    struct pair
    {
        key first;
        value second;
    };

    typedef key key_type;
    typedef value mapped_type;
    typedef vector<pair>::value_type value_type;
    typedef vector<pair>::iterator iterator;
    typedef vector<pair>::const_iterator const_iterator;
    typedef vector<pair>::reverse_iterator reverse_iterator;

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
    const value& at(const key& k) const {
        auto it = lower_bound(k);
        if(unlikely(it == data.end() || it->first != k))
            throw std::runtime_error("fmap::at() error");
        return it->second;
    }
    value& at(const key& k) {
        return const_cast<value&>(const_cast<const fmap*>(this)->at(k));
    }
    value& operator[](const key_type& k) {
        auto it = lower_bound(k);
        if(it == data.end() || it->first != k) {
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
        return lower_bound_simple(data.begin(), data.end(), k,
            [](const pair& v, const key_type& k) { return comp()(v.first, k); }
        );
    }
    iterator lower_bound(const key& k) {
        return lower_bound_simple(data.begin(), data.end(), k,
            [](const pair& v, const key_type& k) { return comp()(v.first, k); }
        );
    }
    const_iterator upper_bound(const key& k) const {
        return upper_bound_simple(data.begin(), data.end(), k,
            [](const key_type& k, const pair& v) { return comp()(k, v.first); }
        );
    }
    iterator upper_bound(const key& k) {
        return iterator(const_cast<const fmap*>(this)->upper_bound(k));
    }
    const_iterator find(const key& k) const {
        auto it = lower_bound(k);
        if(it != data.end() && it->first == k)
            return it;
        return data.end();
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
    reverse_iterator rbegin() const
    {
        return data.rbegin();
    }
    reverse_iterator rend() const
    {
        return data.rend();
    }
    void swap(fmap& r) {
        data.swap(r.data);
    }
    void swap(vector<pair>& r) {
        data.swap(r);
    }
    void insert(const pair& v) {
        auto it = lower_bound(v.first);
        if(it == data.end() || it->first != v.first)
            data.insert(it, v);
    }
    void insert(const key& k, const value_type& v) {
        auto it = lower_bound(k);
        if(it == data.end() || it->first != k)
            it = data.insert(it, v);
    }
    void erase(const key& k) {
        iterator it = find(k);
        data.erase(it);
    }
    iterator erase(const_iterator it) {
        data.erase(iterator(it));
        return iterator(it);
    }
    void erase(const_iterator from, const_iterator to) {
        data.erase(iterator(from), iterator(to));
    }
    void reserve(uint32_t new_capacity) {
        data.reserve(new_capacity);
    }

private:
    vector<pair> data;
};

template<typename key, typename value>
struct pmap : fmap<key, value, std::less<key>, pvector>
{
    using fmap<key, value, std::less<key>, pvector>::fmap;
};

