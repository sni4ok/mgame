/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../makoa/order_book.hpp"

struct mirror
{
    struct impl;
    impl* pimpl;
    
    mirror(str_holder params);
    void proceed(const message* m, u32 count);
    ~mirror();
};

