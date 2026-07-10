/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "stdint.hpp"

void assert_func(char_cit info, char_cit func, char_cit file, int line);

#ifdef NDEBUG
    #define ASSERT(arg)
#else
    #define ASSERT(arg) \
    if(!(arg)) [[unlikely]]\
        assert_func(#arg, __PRETTY_FUNCTION__, __FILE__, __LINE__)
#endif
