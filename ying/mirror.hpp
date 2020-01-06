/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/order_book.hpp"

struct mirror
{
    struct impl;
    
    mirror(const std::string& params); //like "837037107 100"
                                       //  or  "tBTCUSD 100"

    void proceed(const message* m, uint32_t count);
    ~mirror();

private:
    impl* pimpl;
};

