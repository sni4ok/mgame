/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#ifndef __EXCEPTION_H
#define __EXCEPTION_H 1

namespace std
{
    class exception
    {
    public:
        exception() noexcept {}
        virtual ~exception() noexcept;

        exception(const exception&) = default;
        exception& operator=(const exception&) = default;
        exception(exception&&) = default;
        exception& operator=(exception&&) = default;
        virtual const char*
        what() const noexcept;
    };
}

#endif

typedef std::exception exception;

struct str_exception : exception
{
    char_cit msg;

    str_exception(char_cit msg) : msg(msg)
    {
    }
    char_cit what() const noexcept
    {
        return msg;
    }
};

struct bad_alloc : str_exception
{
    bad_alloc() : str_exception("bad_alloc")
    {
    }
};

template<typename stream>
stream& operator<<(stream& s, const exception& e)
{
    s << "exception: " << _str_holder(e.what());
    return s;
}

