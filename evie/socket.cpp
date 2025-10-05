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

int socket_connect(const mstring& host, u16 port, u32 timeout)
{
    bool local = (host == "127.0.0.1") || (host == "localhost");
    int socket = ::socket(AF_INET, local ? AF_LOCAL : SOCK_STREAM /*| SOCK_NONBLOCK*/, IPPROTO_TCP);
    if(socket <= 0)
        throw_system_failure("Open socket error");
    socket_holder sh(socket);
    int flag = 1;
    int result = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char_cit)&flag, sizeof(int));
    if(result < 0)
        throw_system_failure("set socket TCP_NODELAY error");

    sockaddr_in clientService = sockaddr_in(); 
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);

    hostent h, *r;
    char buf[1024];
    int errn;
    int err = gethostbyname_r(host.c_str(), &h, buf, sizeof(buf), &r, &errn);
    if(err)
        throw_system_failure(es() % "gethostbyname error for host: " % host);

    memcpy(&clientService.sin_addr, h.h_addr, h.h_length);
    
    int flags = fcntl(socket, F_GETFL, 0);
    if(fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0)
        throw_system_failure("set O_NONBLOCK for socket error");
    
    result = ::connect(socket, (sockaddr*)&clientService, sizeof(clientService));
    if(errno != EINPROGRESS)
        throw_system_failure(es() % "can't connect to host, err: "
            % result % ": " % host % ":" % port);

    if(result)
    {
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(socket, &fdset);
        timeval tv = timeval();
        tv.tv_sec = timeout;

        result = select(socket + 1, nullptr, &fdset, nullptr, &tv);
        if(result < 0)
            throw_system_failure(es() % "socket_connect select, err: "
                % result % ": " % host % ":" % port);

        if(!result)
            throw mexception("socket_connect timeout");

        int error = 0;
        socklen_t len = sizeof(error);
        if(FD_ISSET(socket, &fdset))
            result = getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len);
        else
            result = -1;
        
        if(result < 0 || error)
            throw_system_failure(es() % "can't connect to host, err: "
                % result % ": " % host % ":" % port);
    }
    if(fcntl(socket, F_SETFL, flags) < 0)
        throw_system_failure("set O_NONBLOCK_2 for socket error");

    return sh.release();
}

void socket_send(int socket, char_cit ptr, u32 sz)
{
    while(sz)
    {
        int ret = ::send(socket, ptr, sz, 0);
        if(ret <= 0)
            throw_system_failure("send eror");
        ptr += ret;
        sz -= ret;
    }
}

u32 try_socket_send(int socket, char_cit ptr, u32 sz)
{
    u32 sended = 0;
    while(sz)
    {
        int ret = ::send(socket, ptr, sz, 0);
        if(ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            //MPROFILE("send_EAGAIN")
            return sended;
        }
        else if(ret == 0)
            throw_system_failure("socket closed");
        else if(ret < 0)
            throw_system_failure("socket error");
        ptr += ret;
        sz -= ret;
        sended += ret;
    }
    return sended;
}

void socket_send_async(int socket, char_cit ptr, u32 sz)
{
    while(sz)
    {
        int ret = ::send(socket, ptr, sz, 0);
        if(ret == -1)
        {
            MPROFILE("send_EAGAIN")
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                pollfd pfd = pollfd();
                pfd.events = POLLOUT;
                pfd.fd = socket;
                ret = poll(&pfd, 1, 100);
                if(ret < 0)
                    throw_system_failure("poll() error");
                //timeout or can send
                continue;
            }
            else
                throw_system_failure("send() error");
        }
        else if(ret == 0)
            throw_system_failure("socket closed");
        else if(ret < 0)
            throw_system_failure("socket error");

        ptr += ret;
        sz -= ret;
    }
}

int socket_accept_async(u32 port, bool local, bool sync, mstring* client_ip_ptr,
    volatile bool* can_run, char_cit name)
{
    int socket = ::socket(AF_INET, local ? AF_LOCAL : SOCK_STREAM /*| SOCK_NONBLOCK*/,
        IPPROTO_TCP);
    if(socket < 0) 
        throw_system_failure(es() % _str_holder(name) % ": Open socket error");
    socket_holder sh(socket);

    int flag = 1;
    if(setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char_cit)&flag, sizeof(int)) < 0)
        throw_system_failure("set socket TCP_NODELAY error");
    if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char_cit)&flag, sizeof(int)) < 0)
        throw_system_failure("set socket SO_REUSEADDR error");

    mlog() << "socket_sender waiting for connection";
    sockaddr_in serv_addr = sockaddr_in(), cli_addr = sockaddr_in();
    socklen_t cli_sz = sizeof(cli_addr);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
   
    if(bind(socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        throw_system_failure("bind() error");
    if(listen(socket, 1) < 0)
        throw_system_failure("listen() error");
    {
        fd_set fdset;
        int result = 0;
        while(!result && (!can_run || *can_run))
        {
            FD_ZERO(&fdset);
            FD_SET(socket, &fdset);
            timeval tv = timeval();
            tv.tv_sec = 1;
        
            result = select(socket + 1, &fdset, nullptr, nullptr, &tv);
            if(result < 0)
                throw_system_failure(es() % "socket_accept_async select, err: " % result);
        }
        if(can_run && !*can_run)
            throw mexception("socket_sender can_run = false reached");
    }

    int socket_cl = accept(socket, (struct sockaddr *)&cli_addr, &cli_sz);
    if(socket_cl < 0)
        throw_system_failure("accept() error");
    socket_holder sc(socket_cl);
    if(setsockopt(socket_cl, IPPROTO_TCP, TCP_NODELAY, (char_cit)&flag, sizeof(int)) < 0)
        throw_system_failure("set socket TCP_NODELAY for client error");
    
    int clip = cli_addr.sin_addr.s_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clip, str, INET_ADDRSTRLEN);

    if(!sync)
    {
        int flags = fcntl(socket_cl, F_GETFL, 0);
        if(fcntl(socket_cl, F_SETFL, flags | O_NONBLOCK) < 0)
            throw_system_failure("set O_NONBLOCK for client socket error");
    }

    mstring client_ip = _str_holder(str);
    mlog() << "socket_sender connection accepted from " << client_ip << ":" << port;
    if(client_ip_ptr)
        *client_ip_ptr = move(client_ip);
    return sc.release();
}

int socket_accept_async(u32 port, const mstring& possible_client_ip, bool sync,
    mstring* client_ip_ptr, volatile bool* can_run, char_cit name)
{
    mstring client_ip;
    int s = socket_accept_async(port, (possible_client_ip == "127.0.0.1"),
        sync, &client_ip, can_run, name);

    if(possible_client_ip != "*" && client_ip != possible_client_ip)
    {
        close(s);
        throw mexception(es() % "socket_sender connected client with ip: " %
            client_ip % ", possible_ip: " % possible_client_ip);
    }
    if(client_ip_ptr)
        *client_ip_ptr = move(client_ip);
    return s;
}

int socket_stream::recv()
{
    pollfd pfd;
    pfd.events = POLLIN;
    pfd.fd = socket;

    int ret = poll(&pfd, 1, timeout);
    if(ret < 0)
        throw_system_failure("socket_stream poll() error");

    if(ret == 0)
    {
        if(op)
            op->on_timeout();
        else
            throw_system_failure("socket_stream poll() timeout error");
        return ret;
    }

    ret = ::recv(socket, cur, all_sz - (cur - beg), 0);
    if(ret == -1)
    {
        MPROFILE("recv_EAGAIN")
        if(errno == EAGAIN || errno == EWOULDBLOCK)
            return ret;
        else
            throw_system_failure("recv() error");
    }
    else if(ret == 0)
        throw_system_failure("socket closed");
    else if(ret < 0)
        throw_system_failure("socket error");

    cur += ret;
    return ret;
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
        throw_system_failure("Open udp socket error");

    socket_holder s(socket);

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr(host.c_str());

    if(bind(socket, (sockaddr*)&sa, sizeof(sa)) < 0)
        throw_system_failure("bind() udp socket error");

    if(!ma.empty())
    {
        ip_mreq_source imr;
        imr.imr_sourceaddr.s_addr = inet_addr(src_ip.c_str());
        imr.imr_multiaddr.s_addr = inet_addr(ma.c_str());
        imr.imr_interface.s_addr = INADDR_ANY;

        if(setsockopt(socket, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
            (char_it)&imr, sizeof(imr)) < 0)
                throw_system_failure("set socket IP_ADD_SOURCE_MEMBERSHIP, error");
    }

    s.release();
}

udp_socket::~udp_socket()
{
    ::close(socket);
}

