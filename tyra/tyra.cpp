/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "tyra.hpp"

#include "../evie/socket.hpp"
#include "../evie/time.hpp"
#include "../evie/mlog.hpp"
#include "../evie/string.hpp"
#include "../evie/algorithm.hpp"

#include <unistd.h>

tyra::tyra(char_cit h) : send_from_call(), send_from_buffer(), bf(buf), bt(buf + sizeof(buf)), c(buf), e(buf) 
{
    str_holder host = _str_holder(h);
    mlog() << "tyra() " << host;
    auto ie = host.end(), i = find(host.begin(), ie, ':');
    if(i == ie || i + 1 == ie)
        throw mexception(es() % "tyra::tyra() bad host: " % host);

    socket = socket_connect({host.begin(), i}, lexical_cast<u16>(i + 1, host.end()));
    mlog() << "tyra() connected to socket " << socket;
}

tyra::~tyra()
{
    mlog() << "~tyra() sfc: " << send_from_call << ", sfb: " << send_from_buffer;
    close(socket);
}

void tyra::send(const message& m)
{
    char_cit ptr = (char_cit)(&m);
    u32 sz = message_size;
    if(c != e)
    {
        if(bt > e + sz) [[unlikely]]
            throw str_exception("tyra::send() message buffer overloaded");
        copy(ptr, ptr + sz, e);
        e += sz;

        u32 s = try_socket_send(socket, c, e - c);
        send_from_buffer += s;
        if(c + s == e)
        {
            c = bf;
            e = bf;
        }
        else
            c += s;
    }
    else
    {
        u32 s = try_socket_send(socket, ptr, sz);
        send_from_call += s;
        if(s != sz)
        {
            copy(ptr + s, ptr + sz, e);
            e += (sz - s);
        }
    }
}

void tyra::send(const message* m, u32 count)
{
    char_cit ptr = (char_cit)(m);
    u32 sz = count * message_size;
    if(c != e)
    {
        if(bt > e + sz) [[unlikely]]
            throw str_exception("tyra::send() messages buffer overloaded");
        copy(ptr, ptr + sz, e);
        e += sz;
        flush();
    }
    else
    {
        u32 s = try_socket_send(socket, ptr, sz);
        send_from_call += s;
        if(s != sz)
        {
            copy(ptr + s, ptr + sz, e);
            e += (sz - s);
        }
    }
}

void tyra::flush()
{
    if(c != e)
    {
        socket_send_async(socket, c, e - c);
        send_from_buffer += (e - c);
        c = bf;
        e = bf;
    }
}

