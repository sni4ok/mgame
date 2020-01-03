/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "utils.hpp"
#include "mlog.hpp"
#include "profiler.hpp"

#include <string>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <netdb.h>


class socket_holder
{
    int s;
    socket_holder(const socket_holder&) = delete;
public:
    socket_holder(int s) : s(s) {
    }
    void free() {
        if(s) {
            close(s);
            s = 0;
        }
    }
    int release() {
        int ret = s;
        s = 0;
        return ret;
    }
    ~socket_holder() {
        if(s)
            close(s);
    }
};

static int socket_connect(const std::string& host, uint16_t port, uint32_t timeout = 3/*in seconds*/)
{
    bool local = (host == "127.0.0.1") || (host == "localhost");
    int socket = ::socket(AF_INET, local ? AF_LOCAL : SOCK_STREAM /*| SOCK_NONBLOCK*/, IPPROTO_TCP);
    if(socket < 0)
        throw_system_failure("Open socket error");
    socket_holder sh(socket);
    int flag = 1;
    int result = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(int));
    if(result < 0)
        throw_system_failure("set socket TCP_NODELAY error");

    sockaddr_in clientService = sockaddr_in(); 
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);

    hostent *sai = gethostbyname(host.c_str());
    if (!sai)
        throw_system_failure(es() % "gethostbyname error for host: " % host);
    
    int flags = fcntl(socket, F_GETFL, 0);
    if(fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0)
        throw_system_failure("set O_NONBLOCK for socket error");
    
    memcpy(&clientService.sin_addr, sai->h_addr, sai->h_length);
    result = ::connect(socket, (sockaddr*)&clientService, sizeof(clientService));
    if(errno != EINPROGRESS)
        throw_system_failure(es() % "can't connect to host, err: " % result % ": " % host % ":" % port);
    if(result != 0) 
    {
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(socket, &fdset);
        timeval tv = timeval();
        tv.tv_sec = timeout;

        if((result = select(socket + 1, NULL, &fdset, NULL, &tv)) < 0)
            throw_system_failure(es() % "socket_connect select, err: " % result % ": " % host % ":" % port);
        if(result == 0)
            throw std::runtime_error("socket_connect timeout");

        int error = 0;
        socklen_t len = sizeof(error);
        if(FD_ISSET(socket, &fdset))
            result = getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len);
        else
            result = -1;
        
        if(result < 0 || error)
            throw_system_failure(es() % "can't connect to host, err: " % result % ": " % host % ":" % port);
    }
    if(fcntl(socket, F_SETFL, flags) < 0)
        throw_system_failure("set O_NONBLOCK_2 for socket error");

    return sh.release();
}

static void socket_send(int socket, const char* ptr, uint32_t sz)
{
    while(sz) {
        int ret = ::send(socket, ptr, sz, 0);
        if(ret <= 0)
            throw_system_failure("send eror");
        ptr += ret;
        sz -= ret;
    }
}

static uint32_t try_socket_send(int socket, const char* ptr, uint32_t sz)
{
    uint32_t sended = 0;
    while(sz) {
        int ret = ::send(socket, ptr, sz, 0);
        if(ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
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

static void socket_send_async(int socket, const char* ptr, uint32_t sz)
{
    while(sz) {
        int ret = ::send(socket, ptr, sz, 0);
        if(ret == -1) {
            MPROFILE("send_EAGAIN")
            pollfd pfd = pollfd();
            pfd.events = POLLOUT;
            pfd.fd = socket;
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                ret = poll(&pfd, 1, 100);
                if(ret < 0)
                    throw_system_failure("poll() error");
                //timeout or can send
                continue;
            } else {
                throw_system_failure("send() error");
            }
        }
        else if(ret == 0)
            throw_system_failure("socket closed");
        else if(ret < 0)
            throw_system_failure("socket error");
        ptr += ret;
        sz -= ret;
    }
}

static void socket_send_async(int socket, buf_stream& ostream)
{
    socket_send_async(socket, ostream.begin(), ostream.size());
    ostream.clear();
}

static std::string name_fmt(const char* name)
{
    if(!name || !strlen(name))
        return std::string();
    return std::string(name) + ": ";
}

static int my_accept_async(uint32_t port, bool local, bool sync = false,
    std::string* client_ip_ptr = nullptr, volatile bool* can_run = nullptr, const char* name = "")
{
    int socket = ::socket(AF_INET, local ? AF_LOCAL : SOCK_STREAM /*| SOCK_NONBLOCK*/, IPPROTO_TCP);
    if(socket < 0) 
        throw_system_failure(es() % name_fmt(name) % "Open socket error");
    socket_holder sh(socket);

    int flag = 1;
    if(setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(int)) < 0)
        throw_system_failure("set socket TCP_NODELAY error");
    if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&flag, sizeof(int)) < 0)
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
        while(!result && (!can_run || *can_run)) {
            FD_ZERO(&fdset);
            FD_SET(socket, &fdset);
            timeval tv = timeval();
            tv.tv_sec = 1;
        
            result = select(socket + 1, &fdset, NULL, NULL, &tv);
            if(result < 0)
                throw_system_failure(es() % "my_accept_async select, err: " % result);
        }
        if(can_run && !*can_run)
            throw std::runtime_error("socket_sender can_run = false reached");
    }
    int socket_cl = accept(socket, (struct sockaddr *)&cli_addr, &cli_sz);
    if(socket_cl < 0)
        throw_system_failure("accept() error");
    socket_holder sc(socket_cl);
    if(setsockopt(socket_cl, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(int)) < 0)
        throw_system_failure("set socket TCP_NODELAY for client error");
    
    int clip = cli_addr.sin_addr.s_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clip, str, INET_ADDRSTRLEN);

    if(!sync) {
        int flags = fcntl(socket_cl, F_GETFL, 0);
        if(fcntl(socket_cl, F_SETFL, flags | O_NONBLOCK) < 0)
            throw_system_failure("set O_NONBLOCK for client socket error");
    }

    std::string client_ip(str);

    mlog() << "socket_sender connection accepted from " << client_ip << ":" << port;
    if(client_ip_ptr)
        *client_ip_ptr = client_ip;
    return sc.release();
}

