/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "../main.hpp"

void proceed_bybit(volatile bool& can_run);

int main(int argc, char** argv)
{
    return parser_main(argc, argv, "bybit", proceed_bybit);
}

