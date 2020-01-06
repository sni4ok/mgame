/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "makoa/messages.hpp"

#include "evie/utils.hpp"

class tyra
{
    uint64_t send_from_call, send_from_buffer;

    int socket;
    char buf[32 * 1024];
    char *bf, *bt;
    char *c, *e; 

    tyra(const tyra&) = delete;
    message_ping mp;

public:
    tyra(const std::string& host);

    void send(const message& m);
    void send(const message* m, uint32_t count);

    template<typename message_type>
    void send(const message_type& m)
    {
        static_assert(sizeof(message_type) == sizeof(message));
        return send(reinterpret_cast<const message&>(m));
    }

    void flush();
    void ping();

    ~tyra();
};

