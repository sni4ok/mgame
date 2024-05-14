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
            value = move(v.value);
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
            value = move(v.value);
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
    bool operator==(const optional& v) const
    {
        if(initialized == v.initialized)
        {
            if(initialized == true)
                return value == v.value;
            return true;
        }
        return false;
    }
    bool operator!=(const optional& v) const
    {
        return !((*this) == v);
    }
    bool operator==(const type& v) const
    {
        if(!initialized)
            return false;
        return value == v;
    }
    bool operator!=(const type& v) const
    {
        return !((*this) == v);
    }
    bool operator!() const
    {
        return !initialized;
    }
};

template<typename stream, typename type>
stream& operator<<(stream& s, const optional<type>& v)
{
    if(!!v)
        s << *v;
    else
        s << "none";
    return s;
}

