/*
    This file contains conversions init_instrument structure to uin32_t and vica versa
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "messages.hpp"

#include <memory>

struct securities 
                  // used modules: logger, config
{
    struct impl;
    securities();
    ~securities();

private:
    std::unique_ptr<impl> pimpl;
};

uint32_t get_security_id(const message_instr& is, bool check_equal); //when check_equal is set function will throw exception
                                                                     //when calculated security_id not equal to received
