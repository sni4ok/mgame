/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "../main.hpp"

void proceed_crypto_com(volatile bool& can_run);

int main(int argc, char** argv)
{
    return parser_main(argc, argv, "crypto.com", proceed_crypto_com);
}

