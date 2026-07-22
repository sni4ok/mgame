/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/time.hpp"

struct price_t
{
    static const i64 exponent = -5;
    i64 value;
};

struct count_t
{
    static const i64 exponent = -8;
    i64 value;
};

struct book_head
{
    price_t price;
};

struct book_leaf
{
    count_t count;
    ttime_t time;
};

struct book : book_head, book_leaf
{
    constexpr bool operator==(const book& r) const
    {
        return price.value == r.price.value
            && count.value == r.count.value
            && time.value == r.time.value;
    }
};

