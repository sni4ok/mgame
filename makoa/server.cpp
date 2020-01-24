/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mmap.hpp"
#include "server.hpp"
#include "engine.hpp"

#include "evie/socket.hpp"

#include <set>
#include <vector>
#include <thread>
#include <condition_variable>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>

static const uint32_t max_connections = 32;
static const uint32_t timeout = 30; //in seconds

const std::string& server_name()
{
    return config::instance().name;
}

str_holder alloc_buffer();
void free_buffer(str_holder buf, context* ctx);
void proceed_data(str_holder& buf, context* ctx);
void* mmap_create(const char* params, bool create);

extern bool pooling_mode;

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

template<typename reader_state>
struct reader
{
    reader_state socket;
    context_holder ctx;
    str_holder buf;
    time_t recv_time;

#if BUFS_CHECK
    bufs_check check;
#endif

    typedef uint32_t (*func)(reader_state socket, char* buf, uint32_t buf_size);
    func read = 0;

    bool proceed()
    {
        uint32_t readed = read(socket, const_cast<char*>(buf.str), buf.size);
        if(unlikely(!readed))
            return false;
        buf.size = readed;
#if BUFS_CHECK
        check.proceed(buf, ctx.ctx);
#else
        proceed_data(buf, ctx.ctx);
#endif
        recv_time = time(NULL);
        return true;
    }
    reader() : buf(alloc_buffer()), recv_time(time(NULL)){
    }
    ~reader() {
        free_buffer(buf, ctx.ctx);
    }
};

uint32_t socket_reader_read(int socket, char* buf, uint32_t buf_size)
{
    int ret = ::recv(socket, buf, buf_size, 0);
    if(unlikely(ret == -1)) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            MPROFILE("server_EAGAIN");
            return 0;
        }
        else
            throw_system_failure("recv() error");
    }
    else if(unlikely(ret == 0))
        throw_system_failure("socket closed");
    else if(unlikely(ret < 0))
        throw_system_failure("socket error");
    return ret;
}

uint32_t pipe_reader_read(int hfile, char* buf, uint32_t buf_size)
{
    uint32_t read_bytes = ::read(hfile, buf, buf_size);
    if(!unlikely(read_bytes))
        throw_system_failure("read() from pipe error");
    return read_bytes;
}

uint32_t mmap_cp_read(void *v, char* buf, uint32_t buf_size)
{
    std::atomic_uchar* f = ((std::atomic_uchar*)v), *e = f + message_size, *i = f;
    uint8_t w = *f;
    uint8_t r = *(f + 1);
    if(unlikely(w < 2 || w > message_size || r < 2 || r > message_size))
        throw std::runtime_error(es() % "mmap_cp_read() internal error, wp: " % uint32_t(w) % ", rp: " % uint32_t(r));

    i += r;
    const message* p = (((const message*)v) + 1) + (r - 2) * 255;
    uint32_t cur_count = atomic_load(i);
    if(cur_count) {
        if(unlikely(buf_size < cur_count))
            throw std::runtime_error(es() % "mmap_cp_read() buf_size too small, wp: " % uint32_t(w) %
                ", rp: " % uint32_t(r) % ", buf_size: " % buf_size % ", cur_count: " % cur_count);
        memcpy(buf, p, cur_count * message_size);
        atomic_store(i, 0);
        ++i;
        if(i == e)
            i = f + 2;
        *(f + 1) = i - f;
        //mlog() << "mmap_cp(" << uint64_t(v) << "," << uint64_t(p) << ",0," << cur_count
        //    << "," << uint32_t(atomic_load(f)) << "," << uint32_t(atomic_load(f + 1)) << ")";
    }
    return cur_count * message_size;
}

struct server::impl : stack_singleton<server::impl>
{
    volatile bool& can_run;

    std::mutex mutex;
    std::condition_variable cond;
    uint32_t count;
    
    impl(volatile bool& can_run) : can_run(can_run), count() {
    }
    void work_thread_impl(int socket, reader<int>::func func)
    {
        pollfd pfd = pollfd();
        pfd.events = POLLIN;
        pfd.fd = socket;

        reader<int> r;
        r.socket = socket;
        r.read = func;

        while(can_run)
        {
            int ret = poll(&pfd, 1, 50);
            if(ret < 0)
                throw_system_failure("poll() error");
            if(ret == 0) {
                //timeout here
                if(time(NULL) > r.recv_time + timeout)
                    throw std::runtime_error("feed timeout");
                continue;
            }
            r.proceed();
        }
    }
    void work_thread_cond(const std::string& params, void* p, reader<void*>::func func)
    {
        reader<void*> rr;
        rr.socket = p;
        rr.read = func;

        shared_memory_sync* s = get_smc(p);
        std::atomic_uchar* w = ((std::atomic_uchar*)p), *r = w + 1;
        bool initialized = false;
        pthread_mutex_lock(&(s->mutex));
        while(!initialized && can_run)
        {
            uint8_t wc = atomic_load(w);
            uint8_t rc = atomic_load(r);
            if(rc != 1 && rc != 0) {
                atomic_store(w, 0);
                pthread_mutex_unlock(&(s->mutex));
                throw std::runtime_error("mmap inconsistence");
            }
            initialized = (wc && rc == 1);
            if(!initialized)
                pthread_cond_wait(&(s->condition), &(s->mutex));
        }
        pthread_mutex_unlock(&(s->mutex));
        *r = 2;
        if(initialized)
        mlog() << "reaceiving data from " << params << " started";

        while(can_run)
        {
            if(!rr.proceed())
            {
                if(!pooling_mode) {
                    if(!pthread_mutex_lock(&(s->mutex)))
                    {
                        uint8_t dr = atomic_load(r);
                        if(dr < 2)
                            throw std::runtime_error("mmap inconsistence 2");

                        std::atomic_uchar *i = w + dr;
                        if(!atomic_load(i)) {
                            pthread_cond_wait(&(s->condition), &(s->mutex));
                            pthread_mutex_unlock(&(s->mutex));
                        }
                    }
                }
            }
        }
    }
    void work_thread(int socket, reader<int>::func func, std::string client, volatile bool& initialized)
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
            work_thread_impl(socket, func);
        } catch(std::exception& e) {
            mlog(mlog::error) << "server(" << server_name() << ") client " << client << " " << e;
        }
        std::unique_lock<std::mutex> lock(mutex);
        cond.notify_all();
        mlog() << "server(" << server_name() << ") thread for " << client << " ended";
        --count;
    }
    void accept_loop(uint16_t port)
    {
        while(can_run) {
            std::string client;
            std::unique_lock<std::mutex> lock(mutex);
            while(can_run&& count >= max_connections)
                cond.wait_for(lock, std::chrono::microseconds(50 * 1000));
            try {
                lock.unlock();
                int socket = my_accept_async(port, false/*local*/, false/*sync*/, &client, &can_run, server_name().c_str());
                volatile bool initialized = false;
                std::thread thrd(&impl::work_thread, this, socket, &socket_reader_read, client, std::ref(initialized));
                lock.lock();
                while(!initialized)
                    cond.wait_for(lock, std::chrono::microseconds(50 * 1000));
                thrd.detach();
            } catch(std::exception& e) {
                mlog(mlog::error) << "server(" << server_name() << ") accept " << e;
                //we don't need infinite log file here
                for(uint i = 0; can_run && i != 5; ++i)
                    sleep(1);
            }
        }
    }
    void import_pipe(std::string params)
    {
        while(can_run) {
            try {
                int r = mkfifo(params.c_str(), 0666);
                my_unused(r);
                int64_t h = open(params.c_str(), O_RDONLY | O_NONBLOCK);
                if(h <= 0)
                    throw_system_failure(es() % "open " % params % " error");
                mlog() << "import from " << params << " started";
                socket_holder ss(h);
                work_thread_impl(h, &pipe_reader_read);
            } catch (std::exception& e) {
                mlog() << "import_pipe " << params << " " << e;
                for(uint i = 0; can_run && i != 5; ++i)
                    sleep(1);
            }
        }
    }
    struct mmap_handle
    {
        void* ptr;
        std::set<void*>& mmaps;
        std::mutex& mutex;
        mmap_handle(const std::string& params, std::set<void*>& mmaps, std::mutex& mutex)
            :  mmaps(mmaps), mutex(mutex)
        {
            std::unique_lock<std::mutex> lock(mutex);
            ptr = mmap_create(params.c_str(), true);
            shared_memory_sync* s = get_smc(ptr);
            //pthread_mutex_lock(&(ps->mutex));
            atomic_store((std::atomic_uchar*)&s->pooling_mode, pooling_mode ? uint8_t(1) : uint8_t(2));
            //pthread_mutex_unlock(&(ps->mutex));
            mmaps.insert(ptr);
        }
        ~mmap_handle()
        {
            mmap_close(ptr);
            std::unique_lock<std::mutex> lock(mutex);
            mmaps.erase(ptr);
        }
    };
    std::set<void*> mmaps;
    void import_mmap_cp(const std::string& params)
    {
        mmap_handle p(params, mmaps, mutex);
        while(can_run) {
            try {
                work_thread_cond(params, p.ptr, &mmap_cp_read);
            } catch (std::exception& e) {
                mlog() << "import_mmap_cp " << params << " " << e;
                for(uint i = 0; can_run && i != 5; ++i)
                    sleep(1);
            }
        }
    }
    void set_close()
    {
        std::unique_lock<std::mutex> lock(mutex);
        for(void* ptr: mmaps) {
            shared_memory_sync* sms = get_smc(ptr);
            if(!pthread_mutex_lock(&(sms->mutex))) {
                pthread_cond_signal(&(sms->condition));
                pthread_mutex_unlock(&(sms->mutex));
            }
        }
    }
    void run()
    {
        for(auto&& i: config::instance().imports) {
            std::vector<std::string> p = split(i, ' ');
            if(p.size() != 2 || (p[0] != "pipe" && p[0] != "tyra" && p[0] != "mmap_cp")) {
                mlog(mlog::critical) << "bad import params: " << i;
                can_run = false;
            }
            else {
                if(p[0] == "tyra")
                    threads.push_back(std::thread(&impl::accept_loop, this, lexical_cast<uint16_t>(p[1])));
                else if(p[0] == "pipe")
                    threads.push_back(std::thread(&impl::import_pipe, this, p[1]));
                else if(p[0] == "mmap_cp")
                    threads.push_back(std::thread(&impl::import_mmap_cp, this, p[1]));
            }
        }
    }
    std::vector<std::thread> threads;
    void join()
    {
        for(auto&& t: threads)
            t.join();
        threads.clear();
    }
    ~impl()
    {
        join();
        std::unique_lock<std::mutex> lock(mutex);
        while(count)
            cond.wait_for(lock, std::chrono::microseconds(50 * 1000));
    }
};

void server_set_close()
{
    server::impl::instance().set_close();
}
void server::import_loop()
{
    pimpl->run();
}
server::server(volatile bool& can_run)
{
    pimpl = std::make_unique<server::impl>(can_run);
}
server::~server()
{
}

