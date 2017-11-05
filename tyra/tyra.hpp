/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/messages.hpp"

#include "evie/utils.hpp"

template<typename message_type>
struct tyra_msg : msg_head, message_type
{
    tyra_msg() : msg_head{message_type::msg_id, message_type::size}
    {
    }
};

class tyra
{
    int socket;
    char buf[32 * 1024];
    buf_stream bs;
    const char* cur; //current pointer on element from buf
    bool need_flush;

    template<typename type> 
    void send_impl(type& t);

    tyra(const tyra&) = delete;
public:
    tyra(const std::string& host);

    uint32_t send_i(tyra_msg<message_instr>& i);

    void send(const tyra_msg<message_instr>& i);
    void send(const tyra_msg<message_trade>& t);
    void send(const tyra_msg<message_clean>& c);
    void send(const tyra_msg<message_book>& b);

    void send(tyra_msg<message_ping>& p); //this method set time by self
    void flush();

    ~tyra();
};

