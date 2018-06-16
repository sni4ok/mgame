/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/messages.hpp"

#include <string>

void* viktor_init(std::string params);
void viktor_destroy(void* v);
void viktor_proceed(void* v, const message& m);

