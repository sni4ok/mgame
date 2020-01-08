/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "tyra.hpp"

#include "evie/socket.hpp"
#include "evie/time.hpp"

tyra::tyra(const std::string& host) : send_from_call(), send_from_buffer(), bf(buf), bt(buf + sizeof(buf)), c(buf), e(buf) 
{
    mlog() << "tyra() " << host;
    auto ie = host.end(), i = std::find(host.begin(), ie, ':');
    if(i == ie || i + 1 == ie)
        throw std::runtime_error(es() % "tyra::tyra() bad host: " % host);

    std::string h(host.begin(), i);
    std::string port(i + 1, host.end());
    socket = socket_connect(h.c_str(), atoi(port.c_str()));
    mlog() << "tyra() connected to socket " << socket;
}

tyra::~tyra()
{
    mlog() << "~tyra() sfc: " << send_from_call << ", sfb: " << send_from_buffer;
    close(socket);
}

void tyra::send(const message& m)
{
    const char* ptr = (const char*)(&m);
    const uint32_t sz = message_size;
    if(c != e) {
        if(unlikely(bt - e > sz))
            throw std::runtime_error("tyra::send() message buffer overloaded");
        std::copy(ptr, ptr + sz, e);
        e += sz;

        uint32_t s = try_socket_send(socket, c, e - c);
        send_from_buffer += s;
        if(s == e - c) {
            c = bf;
            e = bf;
        } else {
            c += s;
        }
    } else {
        uint32_t s = try_socket_send(socket, ptr, sz);
        send_from_call += s;
        if(s != sz) {
            std::copy(ptr + s, ptr + sz, e);
            e += (sz - s);
        }
    }
}

void tyra::send(const message* m, uint32_t count)
{
    const char* ptr = (const char*)(m);
    const uint32_t sz = count * message_size;
    if(c != e) {
        if(unlikely(bt - e > sz))
            throw std::runtime_error("tyra::send() messages buffer overloaded");
        std::copy(ptr, ptr + sz, e);
        e += sz;
        flush();
    } else {
        uint32_t s = try_socket_send(socket, ptr, sz);
        send_from_call += s;
        if(s != sz) {
            std::copy(ptr + s, ptr + sz, e);
            e += (sz - s);
        }
    }
}

void tyra::flush()
{
    if(c != e) {
        socket_send_async(socket, c, e - c);
        send_from_buffer += (e - c);
        c = bf;
        e = bf;
    }
}

