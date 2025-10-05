/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../makoa/order_book.hpp"

struct mirror
{
    struct impl;
    
    mirror(str_holder params); //like "837037107[ 100]"
                               //  or "tBTCUSD[ 100]"
                               //  or "$0[ 100]"

    void proceed(const message* m, u32 count);
    ~mirror();

private:
    impl* pimpl;
};

