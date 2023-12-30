/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

struct none_type
{
};

static const none_type none = none_type();

template<typename type>
struct optional
{
    type value;
    bool initialized;

    optional() : value(), initialized(false)
    {
    }
    optional(const type& v) : value(v), initialized(true)
    {
    }
    optional(const optional& v) : initialized(v.initialized)
    {
        if(initialized)
            value = v.value;
    }
    optional(type&& v) : value(v), initialized(true)
    {
    }
    optional(optional&& v) : initialized(v.initialized)
    {
        if(initialized)
            value = std::move(v.value);
    }
    optional& operator=(const optional& v)
    {
        initialized = v.initialized;
        if(initialized)
            value = v.value;
        return *this;
    }
    optional& operator=(optional&& v)
    {
        initialized = v.initialized;
        if(initialized)
            value = std::move(v.value);
        return *this;
    }
    optional(none_type) : initialized(false)
    {
    }
    const type& operator*() const
    {
        assert(initialized);
        return value;
    }
    type& operator*()
    {
        assert(initialized);
        return value;
    }
    const type* operator->() const
    {
        assert(initialized);
        return &value;
    }
    type* operator->()
    {
        assert(initialized);
        return &value;
    }
    bool operator!() const
    {
        return !initialized;
    }
    operator bool() const
    {
        return initialized;
    }
};

