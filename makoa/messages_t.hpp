/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/time.hpp"

struct price_t
{
    static const i64 exponent = -5;
    static const i64 frac = 100000;
    i64 value;
};

struct count_t
{
    static const i64 exponent = -8;
    static const i64 frac = 100000000;
    i64 value;
};

