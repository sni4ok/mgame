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

int socket_connect(const mstring& host, uint16_t port, uint32_t timeout = 3/*in seconds*/);
void socket_send(int socket, const char* ptr, uint32_t sz);
uint32_t try_socket_send(int socket, const char* ptr, uint32_t sz);
void socket_send_async(int socket, const char* ptr, uint32_t sz);
int my_accept_async(uint32_t port, bool local, bool sync = false, mstring* client_ip_ptr = nullptr,
    volatile bool* can_run = nullptr, const char* name = "");
int my_accept_async(uint32_t port, const mstring& possible_client_ip, bool sync = false, mstring* client_ip_ptr = nullptr,
    volatile bool* can_run = nullptr, const char* name = "");

struct socket_stream_op
{
    virtual void on_compact() = 0;
    virtual void on_timeout() = 0;
};

struct socket_stream
{
    socket_stream_op* op;

    char *cur, *readed, *beg;
    const uint32_t all_sz, timeout;

    int socket;
    bool have_socket;

    socket_stream(uint32_t timeout, char* buf, uint32_t buf_size, int s, bool have_socket = true, socket_stream_op* op = nullptr);
    socket_stream(uint32_t timeout, char* buf, uint32_t buf_size, const mstring& host, uint16_t port, socket_stream_op* op = nullptr);
    int recv();
    ~socket_stream();
    void compact();
    bool get(char& c);
    void read(char* s, uint32_t sz);
};

