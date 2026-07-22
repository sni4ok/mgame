/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "pair.hpp"
#include "new.hpp"
#include "str_holder.hpp"
#include "exception.hpp"
#include "iterator.hpp"
#include "initializer_list.hpp"

void* tss_realloc(void* ptr, u64 old_size, u64 new_size);
void tss_free(void* ptr, u64 size);

template<typename ifrom, typename ito>
constexpr void copy_backward(ifrom from, ifrom to, ito out)
{
    out += (to - from);
    while(from != to)
        *(--out) = *(--to);
}

template<typename type>
inline void copy_backward(type* from, type* to, typename remove_const<type>::type* out)
    requires(is_trivially_destructible<type>::value)
{
    memmove(out, from, (to - from) * sizeof(type));
}

template<typename type, bool tss_allocator = false>
class mvector
{
protected:
    type* buf;
    u64 __size, __capacity;

    void __clear()
    {
        buf = nullptr;
        __size = 0;
        __capacity = 0;
    }

    static const bool have_destructor = !is_trivially_destructible<type>::value;
    static_assert(!__is_polymorphic(type));

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
        ASSERT(it >= buf && it <= buf + __size);
    }

public:

    typedef type* iterator;
    typedef const type* const_iterator;
    typedef type value_type;
    typedef type& reference;

    mvector()
    {
        __clear();
    }
    explicit mvector(u64 size)
    {
        __clear();
        resize(size);
    }
    mvector(mvector&& r)
    {
        __clear();
        swap(r);
    }
    mvector(span<type> str)
    {
        __clear();
        insert(str.begin(), str.end());
    }
    mvector(const type* from, const type* to)
    {
        __clear();
        insert(from, to);
    }
    mvector(const mvector& r)
    {
        __clear();
        insert(r.begin(), r.end());
    }
    mvector(std::initializer_list<type> r)
    {
        __clear();
        insert(r.begin(), r.end());
    }
    mvector& operator=(mvector&& r)
    {
        clear();
        swap(r);
        return *this;
    }
    mvector& operator=(span<type> r)
    {
        clear();
        insert(r.begin(), r.end());
        return *this;
    }
    mvector& operator=(const mvector& r) {
        clear();
        insert(r.begin(), r.end());
        return *this;
    }
    template<bool fill_zero>
    void __resize(u64 new_size)
    {
        if(new_size <= __capacity)
        {
            if(new_size < __size)
                __destroy(begin() + new_size, begin() + __size);
            else if(new_size > __size)
            {
                if constexpr(fill_zero)
                    memset((void*)(buf + __size), 0, (new_size - __size) * sizeof(type));
            }
        }
        else
        {
            void *new_ptr;
            if constexpr(tss_allocator)
                new_ptr = tss_realloc((void*)buf, __capacity * sizeof(type),
                    new_size * sizeof(type));
            else
               new_ptr = realloc((void*)buf, new_size * sizeof(type));
            if(!new_ptr)
                throw bad_alloc();
            buf = (type*)new_ptr;
            if constexpr(fill_zero)
                memset((void*)(buf + __size), 0, (new_size - __size) * sizeof(type));
            __capacity = new_size;
        }
        __size = new_size;
    }
    void resize(u64 new_size)
    {
        __resize<true>(new_size);
    }
    void compact()
    {
        if constexpr(tss_allocator)
            buf = (type*)tss_realloc((void*)buf, __capacity * sizeof(type),
                __size * sizeof(type));
        else
            buf = (type*)realloc((void*)buf, __size * sizeof(type));
        __capacity = __size;
    }
    void swap(mvector& r)
    {
        simple_swap(buf, r.buf);
        simple_swap(__size, r.__size);
        simple_swap(__capacity, r.__capacity);
    }
    void reserve(u64 new_capacity)
    {
        if(new_capacity <= __capacity)
            return;
        void *new_ptr;
        if constexpr(tss_allocator)
            new_ptr = tss_realloc((void*)buf, __capacity * sizeof(type),
                new_capacity * sizeof(type));
        else
            new_ptr = realloc((void*)buf, new_capacity * sizeof(type));
        if(!new_ptr)
            throw bad_alloc();
        buf = (type*)new_ptr;
        __capacity = new_capacity;
    }
    void clear()
    {
        __destroy(begin(), end());
        __size = 0;
    }
    u64 size() const
    {
        return __size;
    }
    bool empty() const
    {
        return !__size;
    }
    u64 capacity() const
    {
        return __capacity;
    }
    const_iterator begin() const
    {
        return buf;
    }
    const_iterator cbegin() const
    {
        return buf;
    }
    iterator begin()
    {
        return buf;
    }
    const_iterator end() const
    {
        return buf + __size;
    }
    const_iterator cend() const
    {
        return buf + __size;
    }
    iterator end()
    {
        return buf + __size;
    }
    span<type> str() const
    {
        return {begin(), end()};
    }

    typedef reverse_iter<const type> const_reverse_iterator;
    typedef reverse_iter<type> reverse_iterator;

    reverse_iterator rbegin()
    {
        return {end() - 1};
    }
    reverse_iterator rend()
    {
        return {begin() - 1};
    }
    const_reverse_iterator rbegin() const
    {
        return {end() - 1};
    }
    const_reverse_iterator rend() const
    {
        return {begin() - 1};
    }
    pair<const_reverse_iterator, const_reverse_iterator> rstr() const
    {
        return {rbegin(), rend()};
    }
    ~mvector()
    {
        __destroy(begin(), end());
        if constexpr(tss_allocator)
            tss_free(buf, __capacity * sizeof(type));
        else
            free(buf);
    }
    type& operator[](u64 elem)
    {
        ASSERT(elem < __size);
        return buf[elem];
    }
    const type& operator[](u64 elem) const
    {
        ASSERT(elem < __size);
        return buf[elem];
    }
    type& at(u64 elem)
    {
        if(elem >= __size)
            throw str_exception("mvector<>::at()");
        return buf[elem];
    }
    const type& at(u64 elem) const
    {
        if(elem >= __size)
            throw str_exception("mvector<>::at()");
        return buf[elem];
    }
    type& back()
    {
        return buf[__size - 1];
    }
    const type& back() const
    {
        return buf[__size - 1];
    }
    void __insert_impl(iterator& it)
    {
        if(__size == __capacity)
        {
            u64 pos = it - buf;
            reserve(__capacity ? __capacity * 2 : 32);
            it = buf + pos;
        }
        __check_iterator(it);
        memmove((void*)(it + 1), it, (end() - it) * sizeof(type));
        if constexpr(have_destructor)
            memset((void*)it, 0, sizeof(type));
    }
    iterator insert(iterator it, type&& v)
    {
        __insert_impl(it);
        *it = ::move(v);
        ++__size;
        return it;
    }
    iterator insert(iterator it, const type& v)
    {
        __insert_impl(it);
        *it = v;
        ++__size;
        return it;
    }
    void insert(const type* from, const type* to)
    {
        u64 size = __size;
        __resize<have_destructor>(to + __size - from);
        copy(from, to, buf + size);
    }
    void insert(iterator it, const type* from, const type* to)
    {
        u64 size = __size;
        u64 pos = it - buf;
        __resize<have_destructor>(to + __size - from);
        it = buf + pos;
        copy_backward(it, buf + size, it + (to - from));
        copy(from, to, it);
    }
    void __push_back_impl()
    {
        if(__size == __capacity)
            reserve(__capacity ? __capacity * 2 : 32);
        if constexpr(have_destructor)
            memset((void*)(buf + __size), 0, sizeof(type));
    }
    void push_back(type&& v)
    {
        __push_back_impl();
        buf[__size] = ::move(v);
        ++__size;
    }
    void push_back(const type& v)
    {
        __push_back_impl();
        buf[__size] = v;
        ++__size;
    }
    template<typename ... params>
    reference emplace_back(params&&... args)
    {
        __push_back_impl();
        new(&buf[__size])type(args...);
        ++__size;
        return buf[__size - 1];
    }
    void pop_back() {
        ASSERT(__size);
        __destroy(begin() + __size - 1);
        --__size;
    }
    iterator erase(iterator it)
    {
        __check_iterator(it);
        __destroy(it);
        memmove((void*)it, it + 1, ((end() - it) - 1) * sizeof(type));
        --__size;
        return it;
    }
    iterator erase(iterator from, iterator to)
    {
        __check_iterator(from);
        __check_iterator(to);
        __destroy(from, to);
        memmove((void*)from, to, (end() - to) * sizeof(type));
        __size -= to - from;
        return from;
    }
    reverse_iterator erase(reverse_iterator it)
    {
        erase(it.ptr);
        --it.ptr;
        return it;
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
        iterator& operator+=(i64 c)
        {
            it += c;
            return *this;
        }
        iterator operator+(i64 c) const
        {
            return iterator({it + c});
        }
        iterator operator-(i64 c) const
        {
            return iterator({it - c});
        }
        i64 operator-(const iterator& r) const
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
        bool operator==(const iterator& r) const
        {
            return it == r.it;
        }
    };

    typedef mvector<type*> base;
    typedef type* value_type;
    typedef iterator const_iterator;

    const_iterator begin() const
    {
        return {base::begin()};
    }
    iterator begin()
    {
        return {base::begin()};
    }
    const_iterator end() const
    {
        return {base::end()};
    }
    iterator end()
    {
        return {base::end()};
    }

    using base::mvector::mvector;
    pvector(const pvector&) = delete;
    pvector& operator=(const pvector&) = delete;

    pvector(pvector&& r)
    {
        base::__clear();
        base::swap(r);
    }
    pvector& operator=(pvector&& r)
    {
        clear();
        static_cast<base&>(*this) = move(r);
        return *this;
    }
    type& operator[](u64 elem)
    {
        return *(base::begin()[elem]);
    }
    const type& operator[](u64 elem) const
    {
        return *(base::begin()[elem]);
    }
    iterator insert(iterator it, type* v)
    {
        ASSERT(v);
        return {base::insert(it.it, v)};
    }
    void splice(pvector& r)
    {
        base::insert(r.begin().it, r.end().it);
        r.base::clear();
    }
    type& back()
    {
        return *(*(base::end() - 1));
    }
    void clear()
    {
        erase(begin(), end());
    }
    iterator erase(iterator from, iterator to)
    {
        for(auto it = from.it; it != to.it; ++it)
            free(*it);
        base::erase(from.it, to.it);
        return from;
    }
    iterator erase(iterator it)
    {
        erase(it, it + 1);
        return it;
    }
    ~pvector()
    {
        clear();
    }
    type** array()
    {
        return this->buf;
    }

private:
    void resize(u64 new_size);
};

template<typename type>
using ppvector = pvector<type>;

struct mexception : exception
{
    mvector<char> msg;

    mexception(str_holder str);
    char_cit what() const noexcept;
};

