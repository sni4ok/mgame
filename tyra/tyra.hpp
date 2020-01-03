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
    uint64_t send_from_call, send_from_buffer;

    int socket;
    char buf[32 * 1024];
    char *bf, *bt;
    char *c, *e; 

    template<typename type> 
    void send_impl(type& t);

    tyra(const tyra&) = delete;
    tyra_msg<message_ping> mp;

public:
    tyra(const std::string& host);

    uint32_t send_i(tyra_msg<message_instr>& i);

    void send(const tyra_msg<message_instr>& i);
    void send(const tyra_msg<message_trade>& t);
    void send(const tyra_msg<message_clean>& c);
    void send(const tyra_msg<message_book>& b);
    
    template<typename message>
    void send(const tyra_msg<message>* messages, uint32_t count);

    void send(tyra_msg<message_ping>& p); //this method set time by self
    void flush();
    void ping();

    ~tyra();
};

