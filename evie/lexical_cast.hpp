/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "stdint.hpp"

struct mlog;
struct mstring;
struct buf_stream;

template<typename type>
type lexical_cast(char_cit it, char_cit ie);

