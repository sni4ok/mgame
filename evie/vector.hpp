/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "pair.hpp"
#include "myitoa.hpp"
#include "type_traits.hpp"

#include <initializer_list>

void* tss_realloc(void* ptr, uint64_t old_size, uint64_t new_size);
void tss_free(void* ptr, uint64_t size);

template<typename ifrom, typename ito>
constexpr void copy(ifrom from, ifrom to, ito out)
{
    for(; from != to; ++from, ++out)
        *out = *from;
}

template<typename type>
inline void copy(type* from, type* to, remove_const_t<type>* out)
    requires(is_trivially_destructible_v<type>)
{
    memcpy(out, from, (to - from) * sizeof(type));
}

template<typename ifrom, typename ito>
constexpr void copy_backward(ifrom from, ifrom to, ito out)
{
    out += (to - from);
    while(from != to)
        *(--out) = *(--to);
}

template<typename type>
inline void copy_backward(type* from, type* to, remove_const_t<type>* out)
    requires(is_trivially_destructible_v<type>)
{
    memmove(out, from, (to - from) * sizeof(type));
}

template<typename type, bool tss_allocator = false>
class mvector
{
protected:
    type* buf;
    uint64_t size_, capacity_;

    void __clear()
    {
        buf = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    static const bool have_destructor = !is_trivially_destructible_v<type>;
    static_assert(!is_polymorphic_v<type>);

    static void __destroy(type* v)
    {
        if constexpr(have_destructor)
            v->type::~type();
    }
    static void __destroy(type* from, type* to)
    {
        if constexpr(have_destructor)
            for(; from != to; ++from)
                from->type::~type();
    }
    void __check_iterator(const type* it)
    {
        (void)it;
        assert(it >= buf && it <= buf + size_);
    }

public:

    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;
    typedef type& reference;

    mvector() {
        __clear();
    }
    explicit mvector(uint64_t size) {
        __clear();
        resize(size);
    }
    mvector(mvector&& r) {
        __clear();
        swap(r);
    }
    mvector(span<type> str) {
        __clear();
        insert(str.begin(), str.end());
    }
    mvector(const type* from, const type* to) {
        __clear();
        insert(from, to);
    }
    mvector(const mvector& r) {
        __clear();
        insert(r.begin(), r.end());
    }
    mvector(std::initializer_list<type> r) {
        __clear();
        insert(r.begin(), r.end());
    }
    mvector& operator=(mvector&& r) {
        clear();
        swap(r);
        return *this;
    }
    mvector& operator=(span<type> r) {
        clear();
        insert(r.begin(), r.end());
        return *this;
    }
    mvector& operator=(const mvector& r) {
        clear();
        insert(r.begin(), r.end());
        return *this;
    }
    mvector& operator=(std::initializer_list<type> r) {
        clear();
        insert(r.begin(), r.end());
        return *this;
    }
    template<bool fill_zero>
    void __resize(uint64_t new_size) {
        if(new_size <= capacity_) {
            if(new_size < size_)
                __destroy(begin() + new_size, begin() + size_);
            else if(new_size > size_) {
                if constexpr(fill_zero)
                    memset((void*)(buf + size_), 0, (new_size - size_) * sizeof(type));
            }
        }
        else
        {
            void *new_ptr;
            if constexpr(tss_allocator)
                new_ptr = tss_realloc((void*)buf, capacity_ * sizeof(type), new_size * sizeof(type));
            else
               new_ptr = realloc((void*)buf, new_size * sizeof(type));
            if(!new_ptr)
                throw std::bad_alloc();
            buf = (type*)new_ptr;
            if constexpr(fill_zero)
                memset((void*)(buf + size_), 0, (new_size - size_) * sizeof(type));
            capacity_ = new_size;
        }
        size_ = new_size;
    }
    void resize(uint64_t new_size) {
        __resize<true>(new_size);
    }
    void compact() {
        if constexpr(tss_allocator)
            buf = (type*)tss_realloc((void*)buf, capacity_ * sizeof(type), size_ * sizeof(type));
        else
            buf = (type*)realloc((void*)buf, size_ * sizeof(type));
        capacity_ = size_;
    }
    void swap(mvector& r) {
        simple_swap(buf, r.buf);
        simple_swap(size_, r.size_);
        simple_swap(capacity_, r.capacity_);
    }
    void reserve(uint64_t new_capacity) {
        if(new_capacity <= capacity_)
            return;
        void *new_ptr;
        if constexpr(tss_allocator)
            new_ptr = tss_realloc((void*)buf, capacity_ * sizeof(type), new_capacity * sizeof(type));
        else
            new_ptr = realloc((void*)buf, new_capacity * sizeof(type));
        if(!new_ptr)
            throw std::bad_alloc();
        buf = (type*)new_ptr;
        capacity_ = new_capacity;
    }
    void clear() {
        __destroy(begin(), end());
        size_ = 0;
    }
    uint64_t size() const {
        return size_;
    }
    bool empty() const {
        return !size_;
    }
    uint64_t capacity() const {
        return capacity_;
    }
    const_iterator begin() const {
        return buf;
    }
    const_iterator cbegin() const {
        return buf;
    }
    iterator begin() {
        return buf;
    }
    const_iterator end() const {
        return buf + size_;
    }
    const_iterator cend() const {
        return buf + size_;
    }
    iterator end() {
        return buf + size_;
    }
    span<type> str() const {
        return {begin(), end()};
    }

    template<typename t>
    struct reverse_iter
    {
        t* ptr;

        reverse_iter& operator++()
        {
            --ptr;
            return *this;
        }
        reverse_iter operator+(int64_t c) const
        {
            return {ptr - c};
        }
        reverse_iter operator-(int64_t c) const
        {
            return {ptr + c};
        }
        int64_t operator-(const reverse_iter& r) const
        {
            return r.ptr - ptr;
        }
        t& operator*()
        {
            return *ptr;
        }
        t* operator->()
        {
            return ptr;
        }
        bool operator!=(const reverse_iter& r) const
        {
            return ptr != r.ptr;
        }
    };

    typedef reverse_iter<const type> const_reverse_iterator;
    typedef reverse_iter<type> reverse_iterator;

    reverse_iterator rbegin() {
        return {end() - 1};
    }
    reverse_iterator rend() {
        return {begin() - 1};
    }
    const_reverse_iterator rbegin() const {
        return {end() - 1};
    }
    const_reverse_iterator rend() const {
        return {begin() - 1};
    }
    pair<const_reverse_iterator, const_reverse_iterator> rstr() const {
        return {rbegin(), rend()};
    }
    ~mvector() {
        __destroy(begin(), end());
        if constexpr(tss_allocator)
            tss_free(buf, capacity_ * sizeof(type));
        else
            free(buf);
    }
    type& operator[](uint64_t elem) {
        assert(elem < size_);
        return buf[elem];
    }
    const type& operator[](uint64_t elem) const {
        assert(elem < size_);
        return buf[elem];
    }
    type& at(uint64_t elem) {
        if(elem >= size_)
            throw str_exception("mvector<>::at()");
        return buf[elem];
    }
    const type& at(uint64_t elem) const{
        if(elem >= size_)
            throw str_exception("mvector<>::at()");
        return buf[elem];
    }
    type& back() {
        return buf[size_ - 1];
    }
    const type& back() const {
        return buf[size_ - 1];
    }
    void __insert_impl(iterator& it) {
        if(size_ == capacity_) {
            uint64_t pos = it - buf;
            reserve(capacity_ ? capacity_ * 2 : 32);
            it = buf + pos;
        }
        __check_iterator(it);
        memmove((void*)(it + 1), it, (end() - it) * sizeof(type));
        if constexpr(have_destructor)
            memset((void*)it, 0, sizeof(type));
    }
    iterator insert(iterator it, type&& v) {
        __insert_impl(it);
        *it = ::move(v);
        ++size_;
        return it;
    }
    iterator insert(iterator it, const type& v) {
        __insert_impl(it);
        *it = v;
        ++size_;
        return it;
    }
    void insert(const type* from, const type* to) {
        uint64_t size = size_;
        __resize<have_destructor>(to + size_ - from);
        copy(from, to, buf + size);
    }
    void insert(iterator it, const type* from, const type* to) {
        uint64_t size = size_;
        uint64_t pos = it - buf;
        __resize<have_destructor>(to + size_ - from);
        it = buf + pos;
        copy_backward(it, buf + size, it + (to - from));
        copy(from, to, it);
    }
    void __push_back_impl() {
        if(size_ == capacity_)
            reserve(capacity_ ? capacity_ * 2 : 32);
        if constexpr(have_destructor)
            memset((void*)(buf + size_), 0, sizeof(type));
    }
    void push_back(type&& v) {
        __push_back_impl();
        buf[size_] = ::move(v);
        ++size_;
    }
    void push_back(const type& v) {
        __push_back_impl();
        buf[size_] = v;
        ++size_;
    }
    template<typename ... params>
    reference emplace_back(params&&... args) {
        __push_back_impl();
        new(&buf[size_])type(args...);
        ++size_;
        return buf[size_ - 1];
    }
    void pop_back() {
        assert(size_);
        __destroy(begin() + size_ - 1);
        --size_;
    }
    iterator erase(iterator it) {
        __check_iterator(it);
        __destroy(it);
        memmove((void*)it, it + 1, ((end() - it) - 1) * sizeof(type));
        --size_;
        return it;
    }
    iterator erase(iterator from, iterator to) {
        __check_iterator(from);
        __check_iterator(to);
        __destroy(from, to);
        memmove((void*)from, to, (end() - to) * sizeof(type));
        size_ -= to - from;
        return from;
    }
};

template<typename type>
using mvector_tss = mvector<type, true>;

template<typename type>
using fvector = mvector<type, false>;

template<typename type>
void pvector_free(type* ptr)
{
    delete ptr;
}

template<typename type>
void pvector_free_array(type* ptr)
{
    delete[] ptr;
}

template<typename type, void (*free)(type*) = pvector_free<type> >
struct pvector : mvector<type*>
{
    struct iterator
    {
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
        return {base::begin()};
    }
    iterator begin() {
        return {base::begin()};
    }
    const_iterator end() const {
        return {base::end()};
    }
    iterator end() {
        return {base::end()};
    }

    using base::mvector::mvector;
    pvector(const pvector&) = delete;
    pvector& operator=(const pvector&) = delete;

    pvector(pvector&& r) {
        base::__clear();
        base::swap(r);
    }
    pvector& operator=(pvector&& r) {
        clear();
        static_cast<base&>(*this) = move(r);
        return *this;
    }
    type& operator[](uint64_t elem) {
        return *(base::begin()[elem]);
    }
    const type& operator[](uint64_t elem) const {
        return *(base::begin()[elem]);
    }
    iterator insert(iterator it, type* v) {
        assert(v);
        return {base::insert(it.it, v)};
    }
    void splice(pvector& r) {
        base::insert(r.begin().it, r.end().it);
        r.base::clear();
    }
    type& back() {
        return *(*(base::end() - 1));
    }
    void clear() {
        erase(begin(), end());
    }
    iterator erase(iterator from, iterator to) {
        for(auto it = from.it; it != to.it; ++it)
            free(*it);
        base::erase(from.it, to.it);
        return from;
    }
    iterator erase(iterator it) {
        erase(it, it + 1);
        return it;
    }
    ~pvector() {
        clear();
    }
    type** array() {
        return this->buf;
    }

private:
    void resize(uint64_t new_size);
};

template<typename type>
using ppvector = pvector<type>;

template<typename type>
struct pavector : pvector<type, pvector_free_array>
{
    typedef pvector<type, pvector_free_array> base;

    type* operator[](uint64_t elem) {
        return &(static_cast<base&>(*this))[elem];
    }
};

struct mexception : exception
{
    mvector<char> msg;

    mexception(str_holder str);
    char_cit what() const noexcept;
};

