/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

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
    void reset(type* ptr = nullptr)
    {
        free(this->ptr);
        this->ptr = ptr;
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
    /*type& operator*()
    {
        return *ptr;
    }
    const type& operator*() const
    {
        return *ptr;
    }*/
    operator bool() const
    {
        return ptr;
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

