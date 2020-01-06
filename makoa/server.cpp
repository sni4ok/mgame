/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "server.hpp"
#include "engine.hpp"

#include <evie/socket.hpp>

#include <vector>
#include <thread>
#include <condition_variable>

static const uint32_t max_connections = 32;
static const uint32_t timeout = 30; //in seconds

const std::string& server_name()
{
    return config::instance().name;
}

str_holder alloc_buffer();
void free_buffer(str_holder buf, context* ctx);
void proceed_data(str_holder& buf, context* ctx);

//#define BUFS_CHECK

#if BUFS_CHECK
struct bufs_check
{
    std::vector<char> data;
    bufs_check() : data(1024 * 1024)
    {
    }

    void proceed(str_holder& buf, context* ctx)
    {
        uint32_t sz = buf.size;
        std::copy(buf.str, buf.str + sz, &data[0]);

        const char* ptr = &data[0];
        while(sz)
        {
            int rnd = rand() % (5 * message_size);
            if(rnd > sz)
                rnd = sz;
            std::copy(ptr, ptr + sz, (char*)buf.str);
            ptr += rnd;
            sz -= rnd;
            buf.size = rnd;
            proceed_data(buf, ctx);
        }

    }
};
#endif

struct server::impl : stack_singleton<server::impl>
{
    volatile bool can_run;

    std::mutex mutex;
    std::condition_variable cond;
    uint32_t count;

    impl() : can_run(true), count() {
    }
    void work_thread_impl(int socket, const std::string&)
    {
        context_holder ctx;
        pollfd pfd = pollfd();
        pfd.events = POLLIN;
        pfd.fd = socket;

        time_t recv_time = time(NULL);
        struct buf_h
        {
            context* ctx;
            str_holder buf;
            buf_h(context* ctx) : ctx(ctx), buf(alloc_buffer()) {
            }
            ~buf_h() {
                free_buffer(buf, ctx);
            }
        };
        buf_h buf(ctx.ctx);

#if BUFS_CHECK
        bufs_check check;
#endif
        while(can_run)
        {
            int ret = poll(&pfd, 1, 50);
            if(ret < 0)
                throw_system_failure("poll() error");
            if(ret == 0) {
                //timeout here
                if(time(NULL) > recv_time + timeout)
                    throw std::runtime_error("feed timeout");

                continue;
            }
            ret = ::recv(socket, (void*)buf.buf.str, buf.buf.size, 0);
            if(ret == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    MPROFILE("server_EAGAIN");
                    continue;
                }
                else
                    throw_system_failure("recv() error");
            }
            else if(ret == 0)
                throw_system_failure("socket closed");
            else if(ret < 0)
                throw_system_failure("socket error");
            buf.buf.size = ret;
#if BUFS_CHECK
            check.proceed(buf.buf, ctx.ctx);
#else
            proceed_data(buf.buf, ctx.ctx);
#endif
            recv_time = time(NULL);
        }
    }
    void work_thread(int socket, std::string client, volatile bool& initialized)
    {
        try {
            socket_holder ss(socket);
            std::unique_lock<std::mutex> lock(mutex);
            ++count;
            initialized = true;
            if(count == max_connections)
                mlog(mlog::warning) << "server(" << server_name() << ") max_connections exceed on client " << client;
            if(count > max_connections)
                throw std::runtime_error("limit connections exceed");
            cond.notify_all();
            lock.unlock();
            mlog() << "server() thread for " << client << " started";
            work_thread_impl(socket, client);
        } catch(std::exception& e) {
            mlog(mlog::error) << "server(" << server_name() << ") client " << client << " " << e;
        }
        std::unique_lock<std::mutex> lock(mutex);
        cond.notify_all();
        mlog() << "server(" << server_name() << ") thread for " << client << " ended";
        --count;
    }
    void accept_loop(volatile bool& can_run_external)
    {
        while(can_run && can_run_external) {
            std::string client;
            std::unique_lock<std::mutex> lock(mutex);
            while(can_run && can_run_external && count >= max_connections)
                cond.wait_for(lock, std::chrono::microseconds(50 * 1000));
            try {
                lock.unlock();
                int socket = my_accept_async(config::instance().port, true/*local*/, false/*sync*/, &client, &can_run_external, server_name().c_str());
                volatile bool initialized = false;
                std::thread thrd(&impl::work_thread, this, socket, client, std::ref(initialized));
                lock.lock();
                while(!initialized)
                    cond.wait_for(lock, std::chrono::microseconds(50 * 1000));
                thrd.detach();
            } catch(std::exception& e) {
                mlog(mlog::error) << "server(" << server_name() << ") accept " << e;
                //we don't need infinite log file here
                if(can_run_external)
                    sleep(5);
            }
        }
    }
    ~impl()
    {
        can_run = false;
        std::unique_lock<std::mutex> lock(mutex);
        while(count)
            cond.wait_for(lock, std::chrono::microseconds(50 * 1000));
    }
};

void server::accept_loop(volatile bool& can_run)
{
    pimpl->accept_loop(can_run);
}
server::server()
{
    pimpl = std::make_unique<server::impl>();
}
server::~server()
{
}

