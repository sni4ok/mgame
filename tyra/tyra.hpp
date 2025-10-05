/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "../makoa/messages.hpp"

class tyra
{
    u64 send_from_call, send_from_buffer;

    int socket;
    char buf[32 * 1024];
    char *bf, *bt;
    char *c, *e; 

    tyra(const tyra&) = delete;

public:
    tyra(const char* host);

    void send(const message& m);
    void send(const message* m, u32 count);

    template<typename message_type>
    void send(const message_type& m)
    {
        static_assert(sizeof(message_type) == sizeof(message));
        return send(reinterpret_cast<const message&>(m));
    }

    void flush();
    ~tyra();
};

