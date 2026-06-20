/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "tyra.hpp"

#include "../evie/socket.hpp"
#include "../evie/mlog.hpp"
#include "../evie/string.hpp"
#include "../evie/algorithm.hpp"

tyra::tyra(char_cit h) : send_from_call(), send_from_buffer(),
    bf(buf), bt(buf + sizeof(buf)), c(buf), e(buf)
{
    socket = socket_connect("export|tcp_client", _str_holder(h));
}

tyra::~tyra()
{
    mlog() << "~export|tcp_client sfc: " << send_from_call << ", sfb: " << send_from_buffer;
    close(socket);
}

void tyra::send(const message* m, u32 count)
{
    char_cit ptr = (char_cit)m;
    u32 sz = count * message_size;
    if(c != e)
    {
        if(bt > e + sz) [[unlikely]]
            throw str_exception("export|tcp_client::send() messages buffer overloaded");
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

