/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "smart_ptr.hpp"
#include "atomic.hpp"

template<typename type, void (*free)(type*) = default_free<type> >
class shared_ptr
{
    type* ptr;
    u32* counter;

    void __free()
    {
        if(counter && !atomic_sub(*counter, 1u))
        {
            free(ptr);
            delete counter;
        }
    }
    void __clear()
    {
        ptr = nullptr;
        counter = nullptr;
    }
    void __reset(type* p)
    {
        if(p)
        {
            ptr = p;
            counter = new u32(1);
        }
        else
            __clear();
    }

public:
    shared_ptr(type* p = nullptr)
    {
        __reset(p);
    }
    shared_ptr(const shared_ptr& r)
    {
        ptr = r.ptr;
        counter = r.counter;
        atomic_add(*counter, 1u);
    }
    shared_ptr(shared_ptr&& r)
    {
        ptr = r.ptr;
        counter = r.counter;
        r.__clear();
    }
    void swap(shared_ptr& r)
    {
        simple_swap(ptr, r.ptr);
        simple_swap(counter, r.counter);
    }
    shared_ptr& operator=(const shared_ptr& r)
    {
        __free();
        ptr = r.ptr;
        counter = r.counter;
        atomic_add(*counter, 1u);
        return *this;
    }
    shared_ptr& operator=(shared_ptr&& r)
    {
        __free();
        ptr = r.ptr;
        counter = r.counter;
        r.__clear();
        return *this;
    }
    void reset(type* p = nullptr)
    {
        __free();
        __reset(p);
    }
    type* get()
    {
        return ptr;
    }
    const type* get() const
    {
        return ptr;
    }
    type* operator->()
    {
        return ptr;
    }
    const type* operator->() const
    {
        return ptr;
    }
    bool operator!() const
    {
        return !ptr;
    }
    ~shared_ptr()
    {
        __free();
    }
};

template<typename type, void (*free)(type*)>
type& operator*(shared_ptr<type, free>& p)
{
    return *p.get();
}

template<typename type, void (*free)(type*)>
const type& operator*(const shared_ptr<type, free>& p)
{
    return *p.get();
}

