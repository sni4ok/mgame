/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef EVIE_SMART_PTR_HPP
#define EVIE_SMART_PTR_HPP

template<typename type>
void default_free(type* ptr)
{
    delete ptr;
}

template<typename type, void (*free)(type*) = default_free<type> >
struct unique_ptr
{
    type* ptr;

    unique_ptr(type* ptr = nullptr) : ptr(ptr)
    {
    }
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr(unique_ptr&& r)
    {
        ptr = r.ptr;
        r.ptr = nullptr;
    }
    void swap(unique_ptr& r)
    {
        simple_swap(ptr, r.ptr);
    }
    unique_ptr& operator=(unique_ptr&& r)
    {
        free(ptr);
        ptr = r.ptr;
        r.ptr = nullptr;
        return *this;
    }
    void reset(type* r = nullptr)
    {
        free(ptr);
        ptr = r;
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
    type* release()
    {
        type* p = ptr;
        ptr = nullptr;
        return p;
    }
    ~unique_ptr()
    {
        free(ptr);
    }
};

template<typename type, void (*free)(type*)>
type& operator*(unique_ptr<type, free>& p)
{
    return *p.ptr;
}

template<typename type, void (*free)(type*)>
const type& operator*(const unique_ptr<type, free>& p)
{
    return *p.ptr;
}

#endif

