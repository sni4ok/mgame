/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "imports.hpp"
#include "mmap.hpp"

#include "../evie/socket.hpp"
#include "../evie/fmap.hpp"
#include "../evie/utils.hpp"
#include "../evie/thread.hpp"
#include "../evie/profiler.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <poll.h>

#include <cerrno>

pair<void*, str_holder> import_context_create(void* params);
void import_context_destroy(pair<void*, str_holder> ctx);
bool import_proceed_data(str_holder& buf, void* ctx);

template<typename reader_state>
struct reader : noncopyable
{
    typedef uint32_t (*func)(reader_state socket, char* buf, uint32_t buf_size);

    reader_state socket;
    func read;
    pair<void*, str_holder> ctx;
    time_t recv_time;

    bool proceed()
    {
        uint32_t readed = read(socket, const_cast<char*>(ctx.second.str), ctx.second.size);
        if(!readed) [[unlikely]]
            return false;
        ctx.second.size = readed;
        bool ret = import_proceed_data(ctx.second, ctx.first);
        recv_time = time(NULL);
        return ret;
    }
    reader(void* ctx_params, reader_state socket, func read) : socket(socket), read(read),
        ctx(import_context_create(ctx_params)), recv_time(time(NULL))
    {
    }
    ~reader()
    {
        import_context_destroy(ctx);
    }
};

uint32_t socket_read(int socket, char* buf, uint32_t buf_size)
{
    int ret = ::recv(socket, buf, buf_size, 0);
    if(ret <= 0) [[unlikely]] {
        if(ret == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                MPROFILE("server_EAGAIN")
                return 0;
            }
            else
                throw_system_failure("recv() error");
        }
        else if(ret == 0)
            throw_system_failure("socket closed");
        else
            throw_system_failure("socket error");
    }
    return ret;
}

uint32_t pipe_read(int hfile, char* buf, uint32_t buf_size)
{
    uint32_t read_bytes = ::read(hfile, buf, buf_size);
    if(!read_bytes) [[unlikely]]
        throw_system_failure("read() from pipe error");
    return read_bytes;
}

uint32_t mmap_cp_read(void *v, char* buf, uint32_t buf_size)
{
    uint8_t* f = (uint8_t*)v, *e = f + message_size, *i = f;
    uint8_t w = mmap_load(f);
    uint8_t r = mmap_load(f + 1);
    if(w < 2 || w >= message_size || r < 2 || r >= message_size) [[unlikely]]
        throw mexception(es() % "mmap_cp_read() internal error, wp: " % uint32_t(w) % ", rp: " % uint32_t(r));

    i += r;
    const message* p = (((const message*)v) + 1) + (r - 2) * 255;
    uint8_t cur_count = mmap_load(i);
    if(cur_count)
    {
        if(buf_size < cur_count) [[unlikely]]
            throw mexception(es() % "mmap_cp_read() buf_size too small, wp: " % uint32_t(w) %
                ", rp: " % uint32_t(r) % ", buf_size: " % buf_size % ", cur_count: " % cur_count);
        memcpy(buf, p, uint32_t(cur_count) * message_size);
        mmap_store(i, 0);
        ++i;
        if(i == e)
            i = f + 2;
        *(f + 1) = uint8_t(i - f);
        //mlog() << "mmap_cp_read(" << w << "," << r << "," << cur_count << "," << uint32_t(*(f)) << "," << uint32_t(*(f + 1)) << ")";
    }
    return cur_count * message_size;
}

struct import_mmap_cp
{
    volatile bool& can_run;
    mvector<mstring> params;
    bool pooling_mode;

    import_mmap_cp(volatile bool& can_run, const mstring& params) : can_run(can_run)
    {
        this->params = split(params, ' ');
        if(this->params.size() != 2)
            throw mexception(es() % " import_mmap_cp() required 2 params (fname,pooling_mode): " % params);
        pooling_mode = lexical_cast<bool>(this->params[1]);
    }
    unique_ptr<void, mmap_close> init() const
    {
        void* p = mmap_create(params[0].c_str(), true);
        unique_ptr<void, &mmap_close> ptr(p);
        shared_memory_sync* s = get_smc(p);
        mmap_store(&s->pooling_mode, pooling_mode ? 1 : 2);

        uint8_t* w = (uint8_t*)p, *r = w + 1;
        bool initialized = false;

        pthread_lock lock(s->mutex);
        while(!initialized && can_run)
        {
            uint8_t wc = mmap_load(w);
            uint8_t rc = mmap_load(r);
            if((rc != 1 && rc != 0) || wc == 1) {
                mmap_store(w, 0);
                throw mexception(es() % "mmap inconsistence, wp: " % wc % ", rp: " % rc);
            }
            initialized = (wc == 2 && rc == 1);
            if(!initialized)
                pthread_timedwait(s->condition, s->mutex, 1);
        }
        *r = 2;
        pthread_cond_signal(&(s->condition));
        if(initialized)
            mlog() << "receiving data from " << params[0] << " started";
        return ptr;
    }
};

void import_mmap_cp_start(void* imc, void* params)
{
    import_mmap_cp& ic = *((import_mmap_cp*)(imc));
    auto ptr = ic.init();
    void* p = ptr.get();
    reader<void*> rr(params, p, mmap_cp_read);
    shared_memory_sync* s = get_smc(p);
    uint8_t* w = (uint8_t*)p, *r = w + 1;

    while(ic.can_run)
    {
        if(!rr.proceed())
        {
            if(!ic.pooling_mode)
            {
                if(!pthread_mutex_trylock(&(s->mutex)))
                {
                    uint8_t rc = *r;
                    if(rc < 2) {
                        pthread_mutex_unlock(&(s->mutex));
                        throw mexception(es() % "mmap inconsistence 2, rp: " % rc);
                    }
                    uint8_t count = mmap_load(w + rc);
                    if(!count)
                        pthread_timedwait(s->condition, s->mutex, 1);
                    pthread_mutex_unlock(&(s->mutex));
                }
            }
        }
    }
}

struct import_pipe
{
    volatile bool& can_run;
    mstring params;

    import_pipe(volatile bool& can_run, const mstring& params) : can_run(can_run), params(params)
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
                throw str_exception("feed timeout");
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
    mstring params;
    uint16_t port;
    uint32_t count;
    my_mutex mutex;
    my_condition cond;

    import_tcp(volatile bool& can_run, const mstring& params) : can_run(can_run), params(params), port(lexical_cast<uint16_t>(params)), count()
    {
    }
    ~import_tcp()
    {
        my_mutex::scoped_lock lock(mutex);
        while(count)
            cond.timed_uwait(lock, 50 * 1000);
    }
};

void import_tcp_thread(import_tcp* it, int socket, mstring client, volatile bool& initialized, void* p)
{
    try {
        socket_holder ss(socket);
        my_mutex::scoped_lock lock(it->mutex);
        ++(it->count);
        initialized = true;
        if(it->count == max_connections)
            mlog(mlog::warning) << "server(" << it->params << ") max_connections exceed on client " << client;
        if(it->count > max_connections)
            throw str_exception("limit connections exceed");
        it->cond.notify_all();
        lock.unlock();
        mlog() << "server() thread for " << client << " started";
        reader<int> r(p, socket, socket_read);
        work_thread_reader(r, it->can_run, timeout);
    } catch(exception& e) {
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
        mstring client;
        my_mutex::scoped_lock lock(it.mutex);
        while(it.can_run && it.count >= max_connections)
            it.cond.timed_uwait(lock, 50 * 1000);
        lock.unlock();
        int socket = my_accept_async(it.port, false/*local*/, false/*sync*/, &client, &it.can_run, it.params.c_str());
        volatile bool initialized = false;
        thread thrd(&import_tcp_thread, &it, socket, client, ref(initialized), p);
        lock.lock();
        while(!initialized)
            it.cond.timed_uwait(lock, 50 * 1000);
        thrd.detach();
    }
}

struct import_ifile
{
    volatile bool& can_run;
    void* ptr;

    import_ifile(volatile bool& can_run, const mstring& params) : can_run(can_run)
    {
        ptr = ifile_create(params.c_str(), can_run);
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
        if(!ret) [[unlikely]]
            return;
    }
}

template<typename type>
void* importer_init(volatile bool& can_run, const char* params)
{
    return (void*)(new type(can_run, _mstring(params)));
}

template<typename type>
void importer_destroy(void* ptr)
{
    delete (type*)ptr;
}

fmap<mstring, hole_importer> _importers;

int register_importer(const char* s, hole_importer hi)
{
    mstring name(_str_holder(s));
    auto it = _importers.find(name);
    if(it != _importers.end())
        throw mexception(es() % "register_importer() double registration for " % name);
    _importers[name] = hi;
    return _importers.size();
}

static const int _import_mmap_cp = register_importer("mmap_cp",
    {importer_init<import_mmap_cp>, importer_destroy<import_mmap_cp>, import_mmap_cp_start, nullptr}
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

hole_importer create_importer(const char* s)
{
    mstring name(_str_holder(s));
    auto it = _importers.find(name);
    if(it == _importers.end())
        throw mexception(es() % "import " % name % " not registered");
    return it->second;
}

