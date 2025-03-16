/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../evie/decimal.hpp"
#include "../evie/time.hpp"

struct price_t
{
    static const int64_t exponent = -5;
    static const int64_t frac = 100000;
    int64_t value;
};

struct count_t
{
    static const int64_t exponent = -8;
    static const int64_t frac = 100000000;
    int64_t value;
};


