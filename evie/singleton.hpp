/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <cassert>

template<typename base, bool reinit_en = true>
class stack_singleton
{
    static base*& get_impl()
    {
        static base* value = 0;
        return value;
    }
    stack_singleton(const stack_singleton&) = delete;

public:
    static void set_instance(base* instance)
    {
        assert((!get_impl() || get_impl() == instance) && "singleton already initialized");
        get_impl() = instance;
    }
    static base& instance()
    {
        return *get_impl();
    }
    stack_singleton()
    {
        set_instance(static_cast<base*>(this));
    }
    ~stack_singleton()
    {
        if constexpr(reinit_en)
            get_impl() = 0;
        else
            get_impl() = (base*)1;
    }
};

