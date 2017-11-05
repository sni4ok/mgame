/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "tyra.hpp"

#include "evie/socket.hpp"
#include "evie/time.hpp"

tyra::tyra(const std::string& host) : bs(buf, buf + sizeof(buf)), cur(buf), need_flush(false) 
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
    mlog() << "~tyra()";
    close(socket);
}

template<typename type> void tyra::send_impl(type& t)
{
    const char* ptr = (const char*) (&t);
    const uint32_t sz = sizeof(t);

    if(need_flush) {
        bs.write(ptr, sz);
        uint32_t s_s = bs.size() - (cur - &buf[0]);
        //mlog() << "p1 " << print_binary((uint8_t*)cur, std::min<int>(24, sz));
        uint32_t s = try_socket_send(socket, cur, s_s);
        if(s == s_s) {
            cur = &buf[0];
            bs.clear();
            need_flush = false;
        } else {
            cur += s;
        }
    } else {
        //mlog() << "p2 " << print_binary((uint8_t*)ptr, std::min<int>(24, sz));
        uint32_t s = try_socket_send(socket, ptr, sz);
        if(s != sz) {
            assert(!bs.size() && cur == &buf[0]);
            bs.write(ptr + s, sz - s);
            need_flush = true;
        }
    }
}

void tyra::flush()
{
    if(need_flush) {
        const char* ptr = cur;
        uint32_t s_s = bs.size() - (cur - &buf[0]);
        cur = &buf[0];
        bs.clear();
        need_flush = false;
        //mlog() << "p3 " << print_binary((uint8_t*)ptr, std::min<int>(24, s_s));
        socket_send_async(socket, ptr, s_s);
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

void tyra::send(tyra_msg<message_ping>& p)
{
    p.time = get_cur_ttime();
    send_impl(p);
    flush();
}

