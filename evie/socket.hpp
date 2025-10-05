/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "mstring.hpp"

class socket_holder
{
    int s;

public:
    socket_holder(int s);
    socket_holder(socket_holder&&) = delete;
    void free();
    int release();
    ~socket_holder();
};

int socket_connect(const mstring& host, u16 port, u32 timeout = 3/*in seconds*/);
void socket_send(int socket, char_cit ptr, u32 sz);
u32 try_socket_send(int socket, char_cit ptr, u32 sz);
void socket_send_async(int socket, char_cit ptr, u32 sz);
int socket_accept_async(u32 port, bool local, bool sync = false,
    mstring* client_ip_ptr = nullptr, volatile bool* can_run = nullptr, char_cit name = "");
int socket_accept_async(u32 port, const mstring& possible_client_ip, bool sync = false,
    mstring* client_ip_ptr = nullptr, volatile bool* can_run = nullptr, char_cit name = "");

struct socket_stream_op
{
    virtual void on_compact() = 0;
    virtual void on_timeout() = 0;
};

struct socket_stream
{
    socket_stream_op* op;
    char *cur, *readed, *beg;
    const u32 all_sz, timeout;
    int socket;

    socket_stream(u32 timeout, char_it buf, u32 buf_size, int s,
        socket_stream_op* op = nullptr);
    socket_stream(u32 timeout, char_it buf, u32 buf_size, const mstring& host,
        u16 port, socket_stream_op* op = nullptr);
    int recv();
    ~socket_stream();
    void compact();
    bool get(char& c);
    void read(char_it s, u32 sz);
};

struct udp_socket
{
    int socket;

    udp_socket(const udp_socket&) = delete;
    udp_socket(const mstring& host, u16 port, const mstring& src_ip = "",
        const mstring& ma = "");
    ~udp_socket();
};

