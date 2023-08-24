/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "myitoa.hpp"

#include <string.h>

template<typename type>
class mvector
{
    type* buf;
    uint32_t size_, capacity_;

public:
    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;

    mvector() : buf(), size_(), capacity_() {
    }
    explicit mvector(uint32_t size) : buf(), size_(), capacity_() {
        resize(size);
        memset(buf, 0, size * sizeof(type));
    }
    mvector(mvector&& r) : buf(r.buf), size_(r.size_), capacity_(r.capacity_) {
        r.buf = 0;
        r.size_ = 0;
        r.capacity_ = 0;
    }
    mvector(const type* from, const type* to): buf(), size_(), capacity_() {
        resize(to - from);
        memmove((void*)&buf[0], from, (to - from) * sizeof(type));
    }
    mvector(const mvector& r) : buf(), size_(), capacity_() {
        resize(r.size());
        memmove((void*)&buf[0], &r.buf[0], (r.size()) * sizeof(type));
    }
    mvector& operator=(const mvector& r) {
        resize(r.size());
        memmove((void*)&buf[0], &r.buf[0], (r.size()) * sizeof(type));
        return *this;
    }
    void resize(uint32_t new_size) {
        if(new_size == size_)
            return;
        if(new_size < size_)
            size_ = new_size;
        if(new_size <= capacity_)
        {
            memset((void*)(buf + size_), 0, (new_size - size_) * sizeof(type));
            size_ = new_size;
        }
        else {
            void *new_ptr = realloc((void*)buf, new_size * sizeof(type));
            if(!new_ptr)
                throw std::bad_alloc();
            buf = (type*)new_ptr;
            memset((void*)(buf + size_), 0, (new_size - size_) * sizeof(type));
            size_ = new_size;
            capacity_ = new_size;
        }
    }
    void swap(mvector& r) {
        std::swap(buf, r.buf);
        std::swap(size_, r.size_);
        std::swap(capacity_, r.capacity_);
    }
    void reserve(uint32_t new_capacity) {
        if(new_capacity <= capacity_)
            return;
        void *new_ptr = realloc((void*)buf, new_capacity * sizeof(type));
        if(!new_ptr)
            throw std::bad_alloc();
        buf = (type*)new_ptr;
        capacity_ = new_capacity;
    }
    void clear() {
        resize(0);
    }
    uint32_t size() const {
        return size_;
    }
    bool empty() const {
        return !size_;
    }
    uint32_t capacity() const {
        return capacity_;
    }
    const_iterator begin() const {
        return buf;
    }
    iterator begin() {
        return buf;
    }
    const_iterator end() const {
        return begin() + size_;
    }
    iterator end() {
        return begin() + size_;
    }
    struct reverse_iterator
    {
        const type* ptr;
        reverse_iterator& operator++()
        {
            --ptr;
            return *this;
        }
        const type& operator*()
        {
            return *ptr;
        }
        const type* operator->()
        {
            return ptr;
        }
        bool operator!=(const reverse_iterator& r) const
        {
            return ptr != r.ptr;
        }
    };
    reverse_iterator rbegin() const
    {
        return reverse_iterator({end() - 1});
    }
    reverse_iterator rend() const
    {
        return reverse_iterator({begin() - 1});
    }
    ~mvector() {
        free(buf);
    }
    type& operator[](uint32_t elem) {
        return begin()[elem];
    }
    const type& operator[](uint32_t elem) const {
        return begin()[elem];
    }
    bool operator==(const mvector& r) const {
        if(size_ == r.size_)
            return !bcmp(buf, r.buf, size_ * sizeof(type));
        return false;
    }
    bool operator!=(const mvector& r) const {
        return !(*this == r);
    }
    iterator insert(iterator it, const type& v) {
        if(size_ == capacity_) {
            uint32_t pos = it - buf;
            reserve(capacity_ ? capacity_ * 2 : 32);
            it = &buf[pos];
        }
        memmove((void*)(it + 1), it, (end() - it) * sizeof(type));
        *it = v;
        ++size_;
        return it;
    }
    void insert(const type* from, const type* to) {
        uint32_t size = size_;
        resize(size_ + (to - from));
        memmove((void*)&buf[size], from, (to - from) * sizeof(type));
    }
    void push_back(const type& v) {
        if(size_ == capacity_)
            reserve(capacity_ ? capacity_ * 2 : 32);
        buf[size_] = v;
        ++size_;
    }
    void erase(iterator it) {
        memmove((void*)it, it + 1, ((end() - it) - 1) * sizeof(type));
        --size_;
    }
    void erase(iterator from, iterator to) {
        memmove((void*)from, to, (end() - to) * sizeof(type));
        size_ -= to - from;
    }
};

template<typename type>
struct pvector : mvector<type*>
{
    struct iterator
    {
        typedef int64_t difference_type;
        typedef type value_type;
        typedef type* pointer;
        typedef type& reference;
        typedef std::random_access_iterator_tag iterator_category;

        type** it;
        iterator() : it()
        {
        }
        iterator(type* const * it) : it(const_cast<type**>(it))
        {
        }
        iterator& operator++()
        {
            ++it;
            return *this;
        }
        iterator& operator--()
        {
            --it;
            return *this;
        }
        iterator& operator+=(int64_t c)
        {
            it += c;
            return *this;
        }
        iterator operator+(int64_t c) const
        {
            return iterator({it + c});
        }
        iterator operator-(int64_t c) const
        {
            return iterator({it - c});
        }
        int64_t operator-(const iterator& r) const
        {
            return it - r.it;
        }
        type& operator*()
        {
            return *(*it);
        }
        type* operator->()
        {
            return *it;
        }
        bool operator!=(const iterator& r) const
        {
            return it != r.it;
        }
        bool operator==(const iterator& r) const
        {
            return it == r.it;
        }
    };

    typedef mvector<type*> base;
    typedef type* value_type;
    typedef iterator const_iterator;

    const_iterator begin() const {
        return const_iterator({base::begin()});
    }
    iterator begin() {
        return iterator({base::begin()});
    }
    const_iterator end() const {
        return const_iterator({base::end()});
    }
    iterator end() {
        return iterator({base::end()});
    }
    pvector()
    {
    }
    type& operator[](uint32_t elem) {
        return *(base::begin()[elem]);
    }
    const type& operator[](uint32_t elem) const{
        return *(base::begin()[elem]);
    }
    iterator insert(iterator it, type* v) {
        if(!v)
            v = new type();
        return {base::insert(it.it, v)};
    }
    void splice(pvector& r)
    {
        base::insert(r.begin().it, r.end().it);
        r.base::resize(0);
    }
    type& back() {
        return *(*(base::end() - 1));
    }
    void clear() {
        erase(begin(), end());
    }
    void erase(iterator from, iterator to) {
        for(auto it = from.it; it != to.it; ++it)
            delete *it;
        base::erase(from.it, to.it);
    }
    void erase(iterator it) {
        erase(it, it + 1);
    }
    ~pvector() {
        clear();
    }

private:
    void resize(uint32_t new_size);
};

