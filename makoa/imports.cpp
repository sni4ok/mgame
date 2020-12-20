/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mmap.hpp"
#include "imports.hpp"

#include "evie/socket.hpp"

#include <map>
#include <atomic>

#include <sys/stat.h>

void* import_context_create(void* params);
str_holder import_alloc_buffer(void* ctx);
void import_free_buffer(str_holder buf, void* ctx);
bool import_proceed_data(str_holder& buf, void* ctx);
void import_context_destroy(void* ctx);

template<typename reader_state>
struct reader : noncopyable
{
    reader_state socket;
    typedef uint32_t (*func)(reader_state socket, char* buf, uint32_t buf_size);
    func read;

    void* ctx;
    str_holder buf;
    time_t recv_time;

    bool proceed()
    {
        uint32_t readed = read(socket, const_cast<char*>(buf.str), buf.size);
        if(unlikely(!readed))
            return false;
        buf.size = readed;
        bool ret = import_proceed_data(buf, ctx);
        recv_time = time(NULL);
        return ret;
    }
    reader(void* ctx_params, reader_state socket, func read) : socket(socket), read(read),
        ctx(import_context_create(ctx_params)), buf(import_alloc_buffer(ctx)), recv_time(time(NULL)){
    }
    ~reader() {
        import_free_buffer(buf, ctx);
        import_context_destroy(ctx);
    }
};

uint32_t socket_read(int socket, char* buf, uint32_t buf_size)
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

uint32_t pipe_read(int hfile, char* buf, uint32_t buf_size)
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
    }
    return cur_count * message_size;
}

struct import_mmap_cp
{
    volatile bool& can_run;
    std::string params;
    void* ptr;
    bool pooling_mode;
    import_mmap_cp(volatile bool& can_run, const std::string& params) : can_run(can_run), params(params)
    {
        std::vector<std::string> p = split(params, ' ');
        if(p.size() != 2)
            throw std::runtime_error(es() % " import_mmap_cp() required 2 params (fname,pooling_mode): " % params);
        pooling_mode = lexical_cast<bool>(p[1]);
        ptr = mmap_create(p[0].c_str(), true);
        shared_memory_sync* s = get_smc(ptr);
        atomic_store((std::atomic_uchar*)&s->pooling_mode, pooling_mode ? uint8_t(1) : uint8_t(2));
    }
    ~import_mmap_cp()
    {
        mmap_close(ptr);
    }
};

void mmap_cp_set_closed(void* imc)
{
    import_mmap_cp& ic = *((import_mmap_cp*)(imc));
    shared_memory_sync* sms = get_smc(ic.ptr);
    if(!pthread_mutex_lock(&(sms->mutex))) {
        pthread_cond_signal(&(sms->condition));
        pthread_mutex_unlock(&(sms->mutex));
    }
}

void import_mmap_cp_start(void* imc, void* params)
{
    import_mmap_cp& ic = *((import_mmap_cp*)(imc));
    reader<void*> rr(params, ic.ptr, mmap_cp_read);
    void* p = ic.ptr;

    shared_memory_sync* s = get_smc(p);
    std::atomic_uchar* w = ((std::atomic_uchar*)p), *r = w + 1;
    bool initialized = false;
    pthread_mutex_lock(&(s->mutex));
    while(!initialized && ic.can_run)
    {
        uint8_t wc = atomic_load(w);
        uint8_t rc = atomic_load(r);
        if(rc != 1 && rc != 0) {
            atomic_store(w, 0);
            pthread_mutex_unlock(&(s->mutex));
            throw std::runtime_error("mmap inconsistence");
        }
        initialized = (wc && rc == 1);
        if(!initialized) {
            timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            t.tv_sec += 1;
            pthread_cond_timedwait(&(s->condition), &(s->mutex), &t);
        }
    }
    pthread_mutex_unlock(&(s->mutex));
    *r = 2;
    if(initialized)
        mlog() << "reaceiving data from " << ic.params << " started";

    while(ic.can_run)
    {
        if(!rr.proceed())
        {
            if(!ic.pooling_mode) {
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

struct import_pipe
{
    volatile bool& can_run;
    std::string params;
    import_pipe(volatile bool& can_run, const std::string& params) : can_run(can_run), params(params)
    {
    }
};

void work_thread_reader(reader<int>& r, volatile bool& can_run, uint32_t timeout/*in seconds*/)
{
    pollfd pfd = pollfd();
    pfd.events = POLLIN;
    pfd.fd = r.socket;

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
        if(!r.proceed())
            break;
    }
}

static const uint32_t max_connections = 32;
static const uint32_t timeout = 30; //in seconds

void import_pipe_start(void* c, void* p)
{
    import_pipe& ip = *((import_pipe*)(c));
    int m = mkfifo(ip.params.c_str(), 0666);
    my_unused(m);
    int64_t h = open(ip.params.c_str(), O_RDONLY | O_NONBLOCK);
    if(h <= 0)
        throw_system_failure(es() % "open " % ip.params % " error");
    mlog() << "import from " << ip.params << " started";
    socket_holder ss(h);
    reader<int> r(p, h, &pipe_read);
    work_thread_reader(r, ip.can_run, timeout);
}

struct import_tcp
{
    volatile bool& can_run;
    std::string params;
    uint16_t port;

    uint32_t count;
    my_mutex mutex;
    my_condition cond;
    import_tcp(volatile bool& can_run, const std::string& params) : can_run(can_run), params(params), port(lexical_cast<uint16_t>(params)), count()
    {
    }
    ~import_tcp()
    {
        my_mutex::scoped_lock lock(mutex);
        while(count)
            cond.timed_uwait(lock, 50 * 1000);
    }
};

void import_tcp_thread(import_tcp* it, int socket, std::string client, volatile bool& initialized, void* p)
{
    try {
        socket_holder ss(socket);
        my_mutex::scoped_lock lock(it->mutex);
        ++(it->count);
        initialized = true;
        if(it->count == max_connections)
            mlog(mlog::warning) << "server(" << it->params << ") max_connections exceed on client " << client;
        if(it->count > max_connections)
            throw std::runtime_error("limit connections exceed");
        it->cond.notify_all();
        lock.unlock();
        mlog() << "server() thread for " << client << " started";
        reader<int> r(p, socket, socket_read);
        work_thread_reader(r, it->can_run, timeout);
    } catch(std::exception& e) {
        mlog(mlog::error) << "server(" << it->params << ") client " << client << " " << e;
    }
    my_mutex::scoped_lock lock(it->mutex);
    it->cond.notify_all();
    mlog() << "server(" << it->params << ") thread for " << client << " ended";
    --(it->count);
}

void import_tcp_start(void* c, void* p)
{
    import_tcp& it = *((import_tcp*)(c));
    while(it.can_run) {
        std::string client;
        my_mutex::scoped_lock lock(it.mutex);
        while(it.can_run && it.count >= max_connections)
            it.cond.timed_uwait(lock, 50 * 1000);
        lock.unlock();
        int socket = my_accept_async(it.port, false/*local*/, false/*sync*/, &client, &it.can_run, it.params.c_str());
        volatile bool initialized = false;
        std::thread thrd(&import_tcp_thread, &it, socket, client, std::ref(initialized), p);
        lock.lock();
        while(!initialized)
            it.cond.timed_uwait(lock, 50 * 1000);
        thrd.detach();
    }
}

void* ifile_create(const char* params);
void ifile_destroy(void *v);
uint32_t ifile_read(void *v, char* buf, uint32_t buf_size);

struct import_ifile
{
    volatile bool& can_run;
    void* ptr;
    import_ifile(volatile bool& can_run, const std::string& params) : can_run(can_run)
    {
        ptr = ifile_create(params.c_str());
    }
    ~import_ifile()
    {
        ifile_destroy(ptr);
    }
};

void import_ifile_start(void* c, void* p)
{
    import_ifile& f = *((import_ifile*)(c));
    reader<void*> r(p, f.ptr, &ifile_read);
    while(f.can_run) {
        bool ret = r.proceed();
        if(unlikely(!ret))
            return;
    }
}

template<typename type>
void* importer_init(volatile bool& can_run, const char* params)
{
    return (void*)(new type(can_run, params));
}

template<typename type>
void importer_destroy(void* ptr)
{
    delete (type*)ptr;
}

std::map<std::string, hole_importer> _importers;

uint32_t register_importer(const std::string& name, hole_importer hi)
{
    _importers[name] = hi;
    return _importers.size();
}

static const int _import_mmap_cp = register_importer("mmap_cp",
    {importer_init<import_mmap_cp>, importer_destroy<import_mmap_cp>, import_mmap_cp_start, mmap_cp_set_closed}
);
static const int _import_pipe = register_importer("pipe",
    {importer_init<import_pipe>, importer_destroy<import_pipe>, import_pipe_start, nullptr}
);
static const int _import_tcp = register_importer("tyra",
    {importer_init<import_tcp>, importer_destroy<import_tcp>, import_tcp_start, nullptr}
);
static const int _import_file = register_importer("file",
    {importer_init<import_ifile>, importer_destroy<import_ifile>, import_ifile_start, nullptr}
);

hole_importer create_importer(const char* name)
{
    auto it = _importers.find(name);
    if(it == _importers.end())
    {
        throw std::runtime_error(es() % "import " % _str_holder(name) % " not registered");
    }
    return it->second;
}

