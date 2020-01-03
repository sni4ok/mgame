/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "../main.hpp"

void proceed_plaza(volatile bool& can_run);

int main(int argc, char** argv)
{
    return parser_main(argc, argv, "plaza", proceed_plaza);
}

