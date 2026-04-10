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

