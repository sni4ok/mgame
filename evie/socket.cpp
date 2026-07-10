/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "socket.hpp"
#include "string.hpp"
#include "profiler.hpp"
#include "mlog.hpp"
#include "algorithm.hpp"

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>

#include <cerrno>

socket_holder::socket_holder(int s) : s(s)
{
}

void socket_holder::free()
{
    if(s)
    {
        close(s);
        s = 0;
    }
}

int socket_holder::release()
{
    int ret = s;
    s = 0;
    return ret;
}

socket_holder::~socket_holder()
{
    if(s)
        close(s);
}

optional<u32> socket_result(int ret, str_holder fname)
{
    if(ret <= 0) [[unlikely]]
    {
        if(ret == -1)
        {
            if(errno == EAGAIN)
            {
                MPROFILE("EAGAIN")
                return none;
            }
            else
                throw_system_failure(es() % fname % ", error");
        }
        else if(!ret)
            throw_system_failure(es() % fname % ", socket closed");
        else
            throw_system_failure(es() % fname % ", socket error");
    }
    return {u32(ret)};
}

bool check_socket(int socket, int events, int timeout_ms, str_holder log_name)
{
    pollfd pfd = pollfd();
    pfd.events = events;
    pfd.fd = socket;

    int res = poll(&pfd, 1, timeout_ms);

    if(!res)
        return false;

    if(res < 0)
        throw_system_failure(es() % log_name % ", poll error");

    if((pfd.revents & POLLERR) || (pfd.revents & POLLHUP))
        throw_system_failure(es() % log_name % ", pollerr || pollhup");

    return true;
}

int socket_connect(const mstring& host, u16 port, u32 timeout, bool wait_pollin)
{
    bool local = (host == "127.0.0.1") || (host == "localhost");
    int socket = ::socket(AF_INET, local ? AF_LOCAL : SOCK_STREAM /*| SOCK_NONBLOCK*/, IPPROTO_TCP);

    if(socket <= 0)
        throw_system_failure("socket_connect, open socket error");

    socket_holder sh(socket);
    int flag = 1;
    int res = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char_cit)&flag, sizeof(int));
    if(res < 0)
        throw_system_failure("socket_connect, set TCP_NODELAY error");

    sockaddr_in clientService = sockaddr_in(); 
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);

    hostent h, *r;
    char buf[1024];
    int errn;
    int err = gethostbyname_r(host.c_str(), &h, buf, sizeof(buf), &r, &errn);
    if(err)
        throw_system_failure(es() % "socket_connect, gethostbyname error: " % host);

    memcpy(&clientService.sin_addr, h.h_addr, h.h_length);
    
    int flags = fcntl(socket, F_GETFL, 0);
    if(fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0)
        throw_system_failure("socket_connect, set O_NONBLOCK error");
    
    res = ::connect(socket, (sockaddr*)&clientService, sizeof(clientService));
    if(errno != EINPROGRESS)
        throw_system_failure(es() % "socket_connect, connect errror: "
            % res % ", " % host % ":" % port);

    if(res)
    {
        auto f = [socket, timeout](int events)
        {
            bool s = check_socket(socket, events, timeout * 1000, "socket_connect");
            if(!s)
                throw mexception("socket_connect, poll timeout");
        };

        f(POLLOUT);
        if(wait_pollin)
            f(POLLIN);
    }

    if(fcntl(socket, F_SETFL, flags) < 0)
        throw_system_failure("socket_connect, set O_NONBLOCK_2 error");

    return sh.release();
}

int socket_connect(str_holder log_name, str_holder host, u32 timeout, bool wait_pollin)
{
    mlog() << log_name << " " << host;
    auto ie = host.end(), i = find(host.begin(), ie, ':');
    if(i == ie || i + 1 == ie)
        throw mexception(es() % log_name % " bad host: " % host);

    int socket = socket_connect({host.begin(), i}, lexical_cast<u16>(i + 1, host.end()), timeout, wait_pollin);
    mlog() << log_name << " connected to socket " << socket;
    return socket;
}

u32 try_socket_send(int socket, char_cit ptr, u32 sz)
{
    u32 sended = 0;
    while(sz)
    {
        int ret = ::send(socket, ptr, sz, 0);
        if(ret > 0)
        {
            ptr += ret;
            sz -= ret;
            sended += ret;
        }
        else
        {
            socket_result(ret, "try_socket_send");
            return sended;
        }
    }
    return sended;
}

void socket_send(int socket, char_cit ptr, u32 sz)
{
    ttime_t send_from = ttime_t();

    for(;;)
    {
        u32 s = try_socket_send(socket, ptr, sz);
        sz -= s;
        if(!sz)
            return;

        ptr += s;
        if(!s)
        {
            if(!!send_from)
            {
                if(cur_ttime_seconds() > send_from + seconds(3))
                    throw str_exception("socket_send, timeout");
            }
            else
                send_from = cur_ttime_seconds();
        }

        check_socket(socket, POLLOUT, 100, "socket_send");
    }
}

int socket_accept(u32 port, bool local, mstring* client_ip_ptr,
    volatile bool* can_run, char_cit name)
{
    int socket = ::socket(AF_INET, local ? AF_LOCAL : SOCK_STREAM /*| SOCK_NONBLOCK*/,
        IPPROTO_TCP);
    if(socket < 0) 
        throw_system_failure(es() % _str_holder(name) % ": Open socket error");
    socket_holder sh(socket);

    int flag = 1;
    if(setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char_cit)&flag, sizeof(int)) < 0)
        throw_system_failure("socket_accept, set socket TCP_NODELAY error");

    if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char_cit)&flag, sizeof(int)) < 0)
        throw_system_failure("socket_accept, set socket SO_REUSEADDR error");

    mlog() << "socket_accept, waiting for connection";
    sockaddr_in serv_addr = sockaddr_in(), cli_addr = sockaddr_in();
    socklen_t cli_sz = sizeof(cli_addr);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
   
    if(bind(socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        throw_system_failure("socket_accept, bind error");

    if(listen(socket, 1) < 0)
        throw_system_failure("socket_accept,listen error");

    pollfd pfd = pollfd();
    pfd.events = POLLIN;
    pfd.fd = socket;

    while(!can_run || *can_run)
    {
        int ret = poll(&pfd, 1, 1000);

        if(ret < 0)
            throw_system_failure("socket_accept, poll error");

        if(ret)
            break;
    }

    if(can_run && !*can_run)
        throw mexception("socket_accept, can_run = false reached");

    int socket_cl = accept(socket, (struct sockaddr *)&cli_addr, &cli_sz);
    if(socket_cl < 0)
        throw_system_failure("socket_accept, accept error");

    socket_holder sc(socket_cl);
    if(setsockopt(socket_cl, IPPROTO_TCP, TCP_NODELAY, (char_cit)&flag, sizeof(int)) < 0)
        throw_system_failure("socket_accept, set socket TCP_NODELAY for client error");
    
    int clip = cli_addr.sin_addr.s_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clip, str, INET_ADDRSTRLEN);

    int flags = fcntl(socket_cl, F_GETFL, 0);
    if(fcntl(socket_cl, F_SETFL, flags | O_NONBLOCK) < 0)
        throw_system_failure("socket_accept, set O_NONBLOCK for client socket error");

    mstring client_ip = _str_holder(str);
    mlog() << "socket_accept, connection accepted from " << client_ip << ":" << port;
    if(client_ip_ptr)
        *client_ip_ptr = move(client_ip);

    return sc.release();
}

int socket_accept(u32 port, const mstring& possible_client_ip,
    mstring* client_ip_ptr, volatile bool* can_run, char_cit name)
{
    mstring client_ip;
    int s = socket_accept(port, possible_client_ip == "127.0.0.1",
        &client_ip, can_run, name);

    if(possible_client_ip != "*" && client_ip != possible_client_ip)
    {
        close(s);
        throw mexception(es() % "socket_accept, connected client with ip: " %
            client_ip % ", possible_ip: " % possible_client_ip);
    }
    if(client_ip_ptr)
        *client_ip_ptr = move(client_ip);

    return s;
}

int socket_stream::recv()
{
    int ret = ::recv(socket, cur, all_sz - (cur - beg), 0);
    if(ret > 0)
    {
        cur += ret;
        return ret;
    }

    pollfd pfd;
    pfd.events = POLLIN;
    pfd.fd = socket;

    ret = poll(&pfd, 1, timeout);
    if(ret > 0)
    {
        ret = ::recv(socket, cur, all_sz - (cur - beg), 0);
        if(ret > 0)
        {
            cur += ret;
            return ret;
        }
        socket_result(ret, "socket_stream");
    }
    else if(!ret)
    {
        if(op)
            op->on_timeout();
        else
            throw_system_failure("socket_stream, timeout error");
    }
    else
        throw_system_failure("socket_stream, poll error");

    return 0;
}

socket_stream::socket_stream(u32 timeout, char_it buf, u32 buf_size,
    int s, socket_stream_op* op) :
    op(op), cur(buf), readed(cur), beg(cur), all_sz(buf_size),
    timeout(timeout), socket(s)
{
}

socket_stream::socket_stream(u32 timeout, char_it buf, u32 buf_size,
    const mstring& host, u16 port, socket_stream_op* op)
    : op(op), cur(buf), readed(cur), beg(cur), all_sz(buf_size),
    timeout(timeout), socket()
{
    socket = socket_connect(host, port);
}

socket_stream::~socket_stream()
{
    ::close(socket);
}

void socket_stream::compact()
{
    if(cur == readed && cur != beg)
    {
        cur = beg;
        readed = beg;
    }
    if(op)
        op->on_compact();
}

bool socket_stream::get(char& c)
{
    while(readed == cur)
        recv();

    c = *readed;
    ++readed;
    compact();
    return true;
}

void socket_stream::read(char_it s, u32 sz)
{
    if(all_sz - (readed - beg) < sz)
    {
        MPROFILE("socket_stream_page_fault")
        for(u32 i = 0; i != sz; ++i)
            get(s[i]);
    }
    else
    {
        while(cur < readed + sz)
            recv();

        memcpy(s, readed, sz);
        readed += sz;
        compact();
    }
}

udp_socket::udp_socket(const mstring& host, u16 port, const mstring& src_ip,
    const mstring& ma)
{
    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(socket < 0)
        throw_system_failure("udp_socket, open socket error");

    socket_holder s(socket);
    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr(host.c_str());

    if(bind(socket, (sockaddr*)&sa, sizeof(sa)) < 0)
        throw_system_failure("udp_socket, bind socket error");

    if(!ma.empty())
    {
        ip_mreq_source imr;
        imr.imr_sourceaddr.s_addr = inet_addr(src_ip.c_str());
        imr.imr_multiaddr.s_addr = inet_addr(ma.c_str());
        imr.imr_interface.s_addr = INADDR_ANY;

        if(setsockopt(socket, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
            (char_it)&imr, sizeof(imr)) < 0)
                throw_system_failure("udp_socket, set socket IP_ADD_SOURCE_MEMBERSHIP, error");
    }

    s.release();
}

udp_socket::~udp_socket()
{
    ::close(socket);
}

