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

template<typename type> void tyra::send_impl(type& t)
{
    const char* ptr = (const char*) (&t);
    const uint32_t sz = sizeof(t);

    if(c != e) {
        if(unlikely(bt - e > sz))
            throw std::runtime_error("tyra::send_impl() buffer overloaded");
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

void tyra::flush()
{
    if(c != e) {
        socket_send_async(socket, c, e - c);
        send_from_buffer += (e - c);
        c = bf;
        e = bf;
    }
}

uint32_t tyra::send_i(tyra_msg<message_instr>& i)
{
    crc32 crc(0);
    const message_instr& m = i;
    crc.process_bytes((const char*)(&m), sizeof(message_instr) - 12);

    i.security_id = crc.checksum();
    send_impl(i);
    return i.security_id;
}

void tyra::send(const tyra_msg<message_instr>& i)
{
    send_impl(i);
}

void tyra::send(const tyra_msg<message_trade>& t)
{
    send_impl(t);
}

void tyra::send(const tyra_msg<message_clean>& c)
{
    send_impl(c);
}

void tyra::send(const tyra_msg<message_book>& b)
{
    send_impl(b);
}

template<typename message>
void tyra::send(const tyra_msg<message>* mb, uint32_t count)
{
    const char* ptr = (const char*)(mb);
    const uint32_t sz = count * sizeof(tyra_msg<message>);
    if(c != e) {
        if(unlikely(bt - e > sz))
            throw std::runtime_error("tyra::send_impl() buffer overloaded");
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

template void tyra::send<message_book>(const tyra_msg<message_book>* mb, uint32_t count);
template void tyra::send<message_trade>(const tyra_msg<message_trade>* mb, uint32_t count);

void tyra::send(tyra_msg<message_ping>& p)
{
    if(c != e) {
        flush();
    }
    else {
        const char* ptr = (const char*)(&p);
        const uint32_t sz = sizeof(p);
        p.time = get_cur_ttime();
        uint32_t s = try_socket_send(socket, ptr, sz);
        send_from_call += s;
        if(s && s != sz) {
            std::copy(ptr + s, ptr + sz, e);
            e += (sz - s);
        }
    }
}

void tyra::ping()
{
    send(mp);
}

