/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/order_book.hpp"

#include <memory>

class mirror
{
    struct impl;
    std::unique_ptr<impl> pimpl;

public:
    mirror(const std::string& params); //like "837037107 100"
                                       //  or  "BTCUSD 100"

    void proceed(const message& m); //proceed call refresh by self, but refresh can be call independently
    void refresh();

    ~mirror();
};

