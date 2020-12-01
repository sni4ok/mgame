/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "myitoa.hpp"

#include <type_traits>

#include "string.h"

template<typename type>
class mvector
{
    type* buf;
    uint32_t size_, capacity_;
    mvector(const mvector&) = delete;

public:
    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;

    mvector() : buf(), size_(), capacity_() {
        //static_assert(std::is_trivially_copy_assignable<type>::value, "mvector::mvector()");
        reserve(64);
    }
    mvector& operator=(const mvector& r)
    {
        resize(r.size());
        memmove(&buf[0], &r.buf[0], (r.size()) * sizeof(type));
        return *this;
    }
    void resize(uint32_t new_size) {
        if(new_size == size_)
            return;
        if(new_size < size_)
            size_ = new_size;
        else if(new_size <= capacity_)
            size_ = new_size;
        else {
            void *new_ptr = realloc(buf, new_size * sizeof(type));
            if(!new_ptr)
                throw std::bad_alloc();
            buf = (type*)new_ptr;
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
        void *new_ptr = realloc(buf, new_capacity * sizeof(type));
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
    const type* begin() const {
        return buf;
    }
    type* begin() {
        return buf;
    }
    const type* end() const {
        return begin() + size_;
    }
    type* end() {
        return begin() + size_;
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
    type* insert(type* it, const type& v) {
        if(size_ == capacity_) {
            uint32_t pos = it - buf;
            reserve(capacity_ * 2);
            it = &buf[pos];
        }
        memmove(it + 1, it, (end() - it) * sizeof(type));
        *it = v;
        ++size_;
        return it;
    }
    void push_back(const type& v) {
        if(size_ == capacity_)
            reserve(capacity_ * 2);
        buf[size_] = v;
        ++size_;
    }
    void erase(type* it) {
        memmove(it, it + 1, ((end() - it) - 1) * sizeof(type));
        --size_;
    }
};

